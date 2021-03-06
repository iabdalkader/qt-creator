/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of Qt Creator.
**
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 3 as published by the Free Software
** Foundation with exceptions as appearing in the file LICENSE.GPL3-EXCEPT
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-3.0.html.
**
****************************************************************************/

#include "qmldesignerplugin.h"
#include "exception.h"
#include "qmldesignerconstants.h"
#include "designmodewidget.h"
#include "settingspage.h"
#include "designmodecontext.h"

#include <connectionview.h>
#include <sourcetool/sourcetool.h>
#include <colortool/colortool.h>
#include <texttool/texttool.h>
#include <pathtool/pathtool.h>

#include <qmljseditor/qmljseditorconstants.h>

#include <qmljstools/qmljstoolsconstants.h>

#include <coreplugin/actionmanager/actionmanager.h>
#include <coreplugin/actionmanager/command.h>
#include <coreplugin/coreconstants.h>
#include <coreplugin/designmode.h>
#include <coreplugin/icore.h>
#include <coreplugin/idocument.h>
#include <coreplugin/modemanager.h>
#include <coreplugin/editormanager/editormanager.h>
#include <extensionsystem/pluginspec.h>
#include <qmljs/qmljsmodelmanagerinterface.h>
#include <projectexplorer/projectexplorerconstants.h>

#include <utils/hostosinfo.h>
#include <utils/qtcassert.h>

#include <QAction>
#include <QTimer>
#include <QCoreApplication>
#include <qplugin.h>
#include <QDebug>
#include <QProcessEnvironment>

namespace QmlDesigner {

class QmlDesignerPluginPrivate {
public:
    ViewManager viewManager;
    DocumentManager documentManager;
    ShortCutManager shortCutManager;

    QMetaObject::Connection rewriterErrorConnection;

    Internal::DesignModeWidget *mainWidget;

    QmlDesigner::PluginManager pluginManager;
    DesignerSettings settings;
    Internal::DesignModeContext *context;
};

QmlDesignerPlugin *QmlDesignerPlugin::m_instance = nullptr;

static bool isInDesignerMode()
{
    return Core::ModeManager::currentMode() == Core::Constants::MODE_DESIGN;
}

static bool checkIfEditorIsQtQuick(Core::IEditor *editor)
{
    if (editor)
    if (editor && editor->document()->id() == QmlJSEditor::Constants::C_QMLJSEDITOR_ID) {
        QmlJS::ModelManagerInterface *modelManager = QmlJS::ModelManagerInterface::instance();
        QmlJS::Document::Ptr document = modelManager->ensuredGetDocumentForPath(editor->document()->filePath().toString());
        if (!document.isNull())
            return document->language() == QmlJS::Dialect::QmlQtQuick1
                    || document->language() == QmlJS::Dialect::QmlQtQuick2
                    || document->language() == QmlJS::Dialect::QmlQtQuick2Ui
                    || document->language() == QmlJS::Dialect::Qml;
    }

    return false;
}

static bool isDesignerMode(Core::Id mode)
{
    return mode == Core::DesignMode::instance()->id();
}

static bool documentIsAlreadyOpen(DesignDocument *designDocument, Core::IEditor *editor, Core::Id newMode)
{
    return designDocument
            && editor == designDocument->editor()
            && isDesignerMode(newMode);
}

static bool shouldAssertInException()
{
    QProcessEnvironment processEnvironment = QProcessEnvironment::systemEnvironment();
    return !processEnvironment.value("QMLDESIGNER_ASSERT_ON_EXCEPTION").isEmpty();
}

static void switchTextDesign()
{
    if (Core::ModeManager::currentMode() == Core::Constants::MODE_EDIT) {
        Core::IEditor *editor = Core::EditorManager::currentEditor();
        if (checkIfEditorIsQtQuick(editor))
            Core::ModeManager::activateMode(Core::Constants::MODE_DESIGN);
    } else if (Core::ModeManager::currentMode() == Core::Constants::MODE_DESIGN) {
        Core::ModeManager::activateMode(Core::Constants::MODE_EDIT);
    }
}

QmlDesignerPlugin::QmlDesignerPlugin()
{
    m_instance = this;
    // Exceptions should never ever assert: they are handled in a number of
    // places where it is actually VALID AND EXPECTED BEHAVIOUR to get an
    // exception.
    // If you still want to see exactly where the exception originally
    // occurred, then you have various ways to do this:
    //  1. set a breakpoint on the constructor of the exception
    //  2. in gdb: "catch throw" or "catch throw Exception"
    //  3. set a breakpoint on __raise_exception()
    // And with gdb, you can even do this from your ~/.gdbinit file.
    // DnD is not working with gdb so this is still needed to get a good stacktrace

    Exception::setShouldAssert(shouldAssertInException());
}

QmlDesignerPlugin::~QmlDesignerPlugin()
{
    if (d) {
        Core::DesignMode::instance()->unregisterDesignWidget(d->mainWidget);
        Core::ICore::removeContextObject(d->context);
        d->context = nullptr;
    }
    delete d;
    d = nullptr;
    m_instance = nullptr;
}

////////////////////////////////////////////////////
//
// INHERITED FROM ExtensionSystem::Plugin
//
////////////////////////////////////////////////////
bool QmlDesignerPlugin::initialize(const QStringList & /*arguments*/, QString *errorMessage/* = 0*/) // =0;
{
    if (!Utils::HostOsInfo::canCreateOpenGLContext(errorMessage))
        return false;

    d = new QmlDesignerPluginPrivate;

    d->settings.fromSettings(Core::ICore::settings());

    d->viewManager.registerViewTakingOwnership(new QmlDesigner::Internal::ConnectionView());
    d->viewManager.registerFormEditorToolTakingOwnership(new QmlDesigner::SourceTool);
    d->viewManager.registerFormEditorToolTakingOwnership(new QmlDesigner::ColorTool);
    d->viewManager.registerFormEditorToolTakingOwnership(new QmlDesigner::TextTool);
    d->viewManager.registerFormEditorToolTakingOwnership(new QmlDesigner::PathTool);

    const Core::Context switchContext(QmlDesigner::Constants::C_QMLDESIGNER,
        QmlJSEditor::Constants::C_QMLJSEDITOR_ID);

    QAction *switchTextDesignAction = new QAction(tr("Switch Text/Design"), this);
    Core::Command *command = Core::ActionManager::registerAction(
                switchTextDesignAction, QmlDesigner::Constants::SWITCH_TEXT_DESIGN, switchContext);
    command->setDefaultKeySequence(QKeySequence(Qt::Key_F4));

    const QString pluginPath = Utils::HostOsInfo::isMacHost()
            ? QString(QCoreApplication::applicationDirPath() + "/../PlugIns/QmlDesigner")
            : QString(QCoreApplication::applicationDirPath() + "/../"
                      + QLatin1String(IDE_LIBRARY_BASENAME) + "/qtcreator/plugins/qmldesigner");
    d->pluginManager.setPluginPaths(QStringList() << pluginPath);

    createDesignModeWidget();
    connect(switchTextDesignAction, &QAction::triggered, this, &switchTextDesign);

    addAutoReleasedObject(new Internal::SettingsPage);

    return true;
}

void QmlDesignerPlugin::extensionsInitialized()
{
    QStringList mimeTypes;
    mimeTypes.append(QmlJSTools::Constants::QML_MIMETYPE);
    mimeTypes.append(QmlJSTools::Constants::QMLUI_MIMETYPE);

    Core::DesignMode::instance()->registerDesignWidget(d->mainWidget, mimeTypes, d->context->context());
    connect(Core::DesignMode::instance(), &Core::DesignMode::actionsUpdated,
        &d->shortCutManager, &ShortCutManager::updateActions);
}

void QmlDesignerPlugin::createDesignModeWidget()
{
    d->mainWidget = new Internal::DesignModeWidget;

    d->context = new Internal::DesignModeContext(d->mainWidget);
    Core::ICore::addContextObject(d->context);
    Core::Context qmlDesignerMainContext(Constants::C_QMLDESIGNER);
    Core::Context qmlDesignerFormEditorContext(Constants::C_QMLFORMEDITOR);
    Core::Context qmlDesignerNavigatorContext(Constants::C_QMLNAVIGATOR);

    d->context->context().add(qmlDesignerMainContext);
    d->context->context().add(qmlDesignerFormEditorContext);
    d->context->context().add(qmlDesignerNavigatorContext);
    d->context->context().add(ProjectExplorer::Constants::LANG_QMLJS);

    d->shortCutManager.registerActions(qmlDesignerMainContext, qmlDesignerFormEditorContext, qmlDesignerNavigatorContext);

    connect(Core::EditorManager::instance(), &Core::EditorManager::currentEditorChanged, [=] (Core::IEditor *editor) {
        if (d && checkIfEditorIsQtQuick(editor) && isInDesignerMode()) {
            d->shortCutManager.updateActions(editor);
            changeEditor();
        }
    });

    connect(Core::EditorManager::instance(), &Core::EditorManager::editorsClosed, [=] (QList<Core::IEditor*> editors) {
        if (d) {
            if (d->documentManager.hasCurrentDesignDocument()
                    && editors.contains(d->documentManager.currentDesignDocument()->textEditor()))
                hideDesigner();

            d->documentManager.removeEditors(editors);
        }
    });

    connect(Core::ModeManager::instance(), &Core::ModeManager::currentModeChanged,
        [=] (Core::Id newMode, Core::Id oldMode) {
        if (d && Core::EditorManager::currentEditor() && checkIfEditorIsQtQuick
                (Core::EditorManager::currentEditor()) && !documentIsAlreadyOpen(
                currentDesignDocument(), Core::EditorManager::currentEditor(), newMode)) {

            if (!isDesignerMode(newMode) && isDesignerMode(oldMode))
                hideDesigner();
            else if (Core::EditorManager::currentEditor() && isDesignerMode(newMode))
                showDesigner();
            else if (currentDesignDocument())
                hideDesigner();
        }
    });
}

void QmlDesignerPlugin::showDesigner()
{
    QTC_ASSERT(!d->documentManager.hasCurrentDesignDocument(), return);

    d->shortCutManager.disconnectUndoActions(currentDesignDocument());
    d->documentManager.setCurrentDesignDocument(Core::EditorManager::currentEditor());
    d->shortCutManager.connectUndoActions(currentDesignDocument());
    d->mainWidget->initialize();

    if (d->documentManager.hasCurrentDesignDocument()) {
        activateAutoSynchronization();
        d->shortCutManager.updateActions(currentDesignDocument()->textEditor());
        d->viewManager.pushFileOnCrumbleBar(d->documentManager.currentDesignDocument()->fileName().fileName());
    }

    d->shortCutManager.updateUndoActions(currentDesignDocument());
}

void QmlDesignerPlugin::hideDesigner()
{
    if (currentDesignDocument()
            && currentModel()
            && !currentDesignDocument()->hasQmlParseErrors())
        jumpTextCursorToSelectedModelNode();

    if (d->documentManager.hasCurrentDesignDocument()) {
        deactivateAutoSynchronization();
        d->mainWidget->saveSettings();
    }

    d->shortCutManager.disconnectUndoActions(currentDesignDocument());
    d->documentManager.setCurrentDesignDocument(nullptr);
    d->shortCutManager.updateUndoActions(nullptr);
}

void QmlDesignerPlugin::changeEditor()
{
    if (d->documentManager.hasCurrentDesignDocument()) {
        deactivateAutoSynchronization();
        d->mainWidget->saveSettings();
    }

    d->shortCutManager.disconnectUndoActions(currentDesignDocument());
    d->documentManager.setCurrentDesignDocument(Core::EditorManager::currentEditor());
    d->mainWidget->initialize();
    d->shortCutManager.connectUndoActions(currentDesignDocument());

    if (d->documentManager.hasCurrentDesignDocument()) {
        activateAutoSynchronization();
        d->viewManager.pushFileOnCrumbleBar(d->documentManager.currentDesignDocument()->fileName().fileName());
        d->viewManager.setComponentViewToMaster();
    }

    d->shortCutManager.updateUndoActions(currentDesignDocument());
}

void QmlDesignerPlugin::jumpTextCursorToSelectedModelNode()
{
    // visual editor -> text editor
    ModelNode selectedNode;
    if (!rewriterView()->selectedModelNodes().isEmpty())
        selectedNode = rewriterView()->selectedModelNodes().first();

    if (selectedNode.isValid()) {
        const int nodeOffset = rewriterView()->nodeOffset(selectedNode);
        if (nodeOffset > 0) {
            const ModelNode currentSelectedNode = rewriterView()->
                nodeAtTextCursorPosition(currentDesignDocument()->plainTextEdit()->textCursor().position());
            if (currentSelectedNode != selectedNode) {
                int line, column;
                currentDesignDocument()->textEditor()->convertPosition(nodeOffset, &line, &column);
                currentDesignDocument()->textEditor()->gotoLine(line, column);
            }
        }
    }
}

void QmlDesignerPlugin::selectModelNodeUnderTextCursor()
{
    const int cursorPosition = currentDesignDocument()->plainTextEdit()->textCursor().position();
    ModelNode modelNode = rewriterView()->nodeAtTextCursorPosition(cursorPosition);
    if (modelNode.isValid())
        rewriterView()->setSelectedModelNode(modelNode);
}

void QmlDesignerPlugin::activateAutoSynchronization()
{
    // text editor -> visual editor
    if (!currentDesignDocument()->isDocumentLoaded())
        currentDesignDocument()->loadDocument(currentDesignDocument()->plainTextEdit());

    currentDesignDocument()->updateActiveQtVersion();
    currentDesignDocument()->attachRewriterToModel();

    resetModelSelection();

    viewManager().attachComponentView();
    viewManager().attachViewsExceptRewriterAndComponetView();

    QList<RewriterError> errors = currentDesignDocument()->qmlParseErrors();
    if (errors.isEmpty()) {
        selectModelNodeUnderTextCursor();
        d->mainWidget->enableWidgets();
        d->mainWidget->setupNavigatorHistory(currentDesignDocument()->textEditor());
    } else {
        d->mainWidget->disableWidgets();
        d->mainWidget->showMessageBox(errors + currentDesignDocument()->qmlParseWarnings());
    }

    currentDesignDocument()->updateSubcomponentManager();

    d->rewriterErrorConnection = connect(rewriterView(), &RewriterView::errorsChanged,
        d->mainWidget, &Internal::DesignModeWidget::updateErrorStatus);
}

void QmlDesignerPlugin::deactivateAutoSynchronization()
{
    viewManager().detachViewsExceptRewriterAndComponetView();
    viewManager().detachComponentView();
    viewManager().detachRewriterView();
    documentManager().currentDesignDocument()->resetToDocumentModel();

    disconnect(d->rewriterErrorConnection);
}

void QmlDesignerPlugin::resetModelSelection()
{
    if (rewriterView() && currentModel())
        rewriterView()->setSelectedModelNodes(QList<ModelNode>());
}

RewriterView *QmlDesignerPlugin::rewriterView() const
{
    return currentDesignDocument()->rewriterView();
}

Model *QmlDesignerPlugin::currentModel() const
{
    return currentDesignDocument()->currentModel();
}

DesignDocument *QmlDesignerPlugin::currentDesignDocument() const
{
    if (d)
        return d->documentManager.currentDesignDocument();

    return nullptr;
}

Internal::DesignModeWidget *QmlDesignerPlugin::mainWidget() const
{
    if (d)
        return d->mainWidget;

    return nullptr;
}

void QmlDesignerPlugin::switchToTextModeDeferred()
{
    QTimer::singleShot(0, this, [] () {
        Core::ModeManager::activateMode(Core::Constants::MODE_EDIT);
    });
}

QmlDesignerPlugin *QmlDesignerPlugin::instance()
{
    return m_instance;
}

DocumentManager &QmlDesignerPlugin::documentManager()
{
    return d->documentManager;
}

const DocumentManager &QmlDesignerPlugin::documentManager() const
{
    return d->documentManager;
}

ViewManager &QmlDesignerPlugin::viewManager()
{
    return d->viewManager;
}

const ViewManager &QmlDesignerPlugin::viewManager() const
{
    return d->viewManager;
}

DesignerActionManager &QmlDesignerPlugin::designerActionManager()
{
    return d->viewManager.designerActionManager();
}

const DesignerActionManager &QmlDesignerPlugin::designerActionManager() const
{
    return d->viewManager.designerActionManager();
}

DesignerSettings QmlDesignerPlugin::settings()
{
    d->settings.fromSettings(Core::ICore::settings());
    return d->settings;
}

void QmlDesignerPlugin::setSettings(const DesignerSettings &s)
{
    if (s != d->settings) {
        d->settings = s;
        d->settings.toSettings(Core::ICore::settings());
    }
}

}
