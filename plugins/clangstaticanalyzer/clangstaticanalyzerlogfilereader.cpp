/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd
** All rights reserved.
** For any questions to The Qt Company, please use contact form at http://www.qt.io/contact-us
**
** This file is part of the Qt Enterprise ClangStaticAnalyzer Add-on.
**
** Licensees holding valid Qt Enterprise licenses may use this file in
** accordance with the Qt Enterprise License Agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company.
**
** If you have questions regarding the use of this file, please use
** contact form at http://www.qt.io/contact-us
**
****************************************************************************/

#include "clangstaticanalyzerlogfilereader.h"

#include <QDebug>
#include <QObject>
#include <QFile>
#include <QFileInfo>
#include <QXmlStreamReader>

#include <utils/qtcassert.h>

namespace ClangStaticAnalyzer {
namespace Internal {

class ClangStaticAnalyzerLogFileReader
{
public:
    ClangStaticAnalyzerLogFileReader(const QString &filePath);

    QXmlStreamReader::Error read();

    // Output
    QString clangVersion() const;
    QStringList files() const;
    QList<Diagnostic> diagnostics() const;

private:
    void readPlist();
    void readTopLevelDict();
    void readDiagnosticsArray();
    void readDiagnosticsDict();
    QList<ExplainingStep> readPathArray();
    ExplainingStep readPathDict();
    Analyzer::DiagnosticLocation readLocationDict(bool elementIsRead = false);
    QList<Analyzer::DiagnosticLocation> readRangesArray();

    QString readString();
    QStringList readStringArray();
    int readInteger(bool *convertedSuccessfully);

private:
    QString m_filePath;
    QXmlStreamReader m_xml;

    QString m_clangVersion;
    QStringList m_referencedFiles;
    QList<Diagnostic> m_diagnostics;
};

QList<Diagnostic> LogFileReader::read(const QString &filePath, QString *errorMessage)
{
    const QList<Diagnostic> emptyList;

    // Check file path
    QFileInfo fi(filePath);
    if (!fi.exists() || !fi.isReadable()) {
        if (errorMessage) {
            *errorMessage = QObject::tr("File \"%1\" does not exist or is not readable.")
                    .arg(filePath);
        }
        return emptyList;
    }

    // Read
    ClangStaticAnalyzerLogFileReader reader(filePath);
    const QXmlStreamReader::Error error = reader.read();

    // Return diagnostics
    switch (error) {
    case QXmlStreamReader::NoError:
        return reader.diagnostics();

    // Handle errors
    case QXmlStreamReader::UnexpectedElementError:
        if (errorMessage) {
            *errorMessage = QObject::tr("Could not read file \"%1\": UnexpectedElementError.")
                                .arg(filePath);
        } // fall-through
    case QXmlStreamReader::CustomError:
        if (errorMessage) {
            *errorMessage = QObject::tr("Could not read file \"%1\": CustomError.")
                                .arg(filePath);
        }  // fall-through
    case QXmlStreamReader::NotWellFormedError:
        if (errorMessage) {
            *errorMessage = QObject::tr("Could not read file \"%1\": NotWellFormedError.")
                                .arg(filePath);
        }  // fall-through
    case QXmlStreamReader::PrematureEndOfDocumentError:
        if (errorMessage) {
            *errorMessage = QObject::tr("Could not read file \"%1\": PrematureEndOfDocumentError.")
                                .arg(filePath);
        } // fall-through
    default:
        return emptyList;
    }
}

ClangStaticAnalyzerLogFileReader::ClangStaticAnalyzerLogFileReader(const QString &filePath)
    : m_filePath(filePath)
{
}

QXmlStreamReader::Error ClangStaticAnalyzerLogFileReader::read()
{
    QTC_ASSERT(!m_filePath.isEmpty(), return QXmlStreamReader::CustomError);
    QFile file(m_filePath);
    QTC_ASSERT(file.open(QIODevice::ReadOnly | QIODevice::Text),
               return QXmlStreamReader::CustomError);

    m_xml.setDevice(&file);
    readPlist();

    // If file is empty, m_xml.error() == QXmlStreamReader::PrematureEndOfDocumentError
    return m_xml.error();
}

QString ClangStaticAnalyzerLogFileReader::clangVersion() const
{
    return m_clangVersion;
}

QStringList ClangStaticAnalyzerLogFileReader::files() const
{
    return m_referencedFiles;
}

QList<Diagnostic> ClangStaticAnalyzerLogFileReader::diagnostics() const
{
    return m_diagnostics;
}

void ClangStaticAnalyzerLogFileReader::readPlist()
{
    if (m_xml.readNextStartElement()) {
        if (m_xml.name() == QLatin1String("plist")) {
            if (m_xml.attributes().value(QLatin1String("version")) == QLatin1String("1.0"))
                readTopLevelDict();
        } else {
            m_xml.raiseError(QObject::tr("File is not a plist version 1.0 file."));
        }
    }
}

void ClangStaticAnalyzerLogFileReader::readTopLevelDict()
{
    QTC_ASSERT(m_xml.isStartElement() && m_xml.name() == QLatin1String("plist"), return);
    QTC_ASSERT(m_xml.readNextStartElement() && m_xml.name() == QLatin1String("dict"), return);

    while (m_xml.readNextStartElement()) {
        if (m_xml.name() == QLatin1String("key")) {
            const QString key = m_xml.readElementText();
            if (key == QLatin1String("clang_version"))
                m_clangVersion = readString();
            else if (key == QLatin1String("files"))
                m_referencedFiles = readStringArray();
            else if (key == QLatin1String("diagnostics"))
                readDiagnosticsArray();
        } else {
            m_xml.skipCurrentElement();
        }
    }
}

void ClangStaticAnalyzerLogFileReader::readDiagnosticsArray()
{
    if (m_xml.readNextStartElement() && m_xml.name() == QLatin1String("array")) {
        while (m_xml.readNextStartElement() && m_xml.name() == QLatin1String("dict"))
            readDiagnosticsDict();
    }
}

void ClangStaticAnalyzerLogFileReader::readDiagnosticsDict()
{
    QTC_ASSERT(m_xml.isStartElement() && m_xml.name() == QLatin1String("dict"), return);

    Diagnostic diagnostic;

    while (m_xml.readNextStartElement()) {
        if (m_xml.name() == QLatin1String("key")) {
            const QString key = m_xml.readElementText();
            if (key == QLatin1String("path"))
                diagnostic.explainingSteps = readPathArray();
            else if (key == QLatin1String("description"))
                diagnostic.description = readString();
            else if (key == QLatin1String("category"))
                diagnostic.category = readString();
            else if (key == QLatin1String("type"))
                diagnostic.type = readString();
            else if (key == QLatin1String("issue_context_kind"))
                diagnostic.issueContextKind = readString();
            else if (key == QLatin1String("issue_context"))
                diagnostic.issueContext = readString();
            else if (key == QLatin1String("location"))
                diagnostic.location = readLocationDict();
        } else {
            m_xml.skipCurrentElement();
        }
    }

    m_diagnostics << diagnostic;
}

QList<ExplainingStep> ClangStaticAnalyzerLogFileReader::readPathArray()
{
    QList<ExplainingStep> result;

    if (m_xml.readNextStartElement() && m_xml.name() == QLatin1String("array")) {
        while (m_xml.readNextStartElement() && m_xml.name() == QLatin1String("dict")) {
            const ExplainingStep step = readPathDict();
            if (step.isValid())
                result << step;
        }
    }

    return result;
}

ExplainingStep ClangStaticAnalyzerLogFileReader::readPathDict()
{
    ExplainingStep explainingStep;

    QTC_ASSERT(m_xml.isStartElement() && m_xml.name() == QLatin1String("dict"),
               return explainingStep);

    // We are interested only in dict entries an kind=event type
    if (m_xml.readNextStartElement()) {
        if (m_xml.name() == QLatin1String("key")) {
            const QString key = m_xml.readElementText();
            QTC_ASSERT(key == QLatin1String("kind"), return explainingStep);
            const QString kind = readString();
            if (kind != QLatin1String("event")) {
                m_xml.skipCurrentElement();
                return explainingStep;
            }
        }
    }

    bool depthOk = false;

    while (m_xml.readNextStartElement()) {
        if (m_xml.name() == QLatin1String("key")) {
            const QString key = m_xml.readElementText();
            if (key == QLatin1String("location"))
                explainingStep.location = readLocationDict();
            else if (key == QLatin1String("ranges"))
                explainingStep.ranges = readRangesArray();
            else if (key == QLatin1String("depth"))
                explainingStep.depth = readInteger(&depthOk);
            else if (key == QLatin1String("message"))
                explainingStep.message = readString();
            else if (key == QLatin1String("extended_message"))
                explainingStep.extendedMessage = readString();
        } else {
            m_xml.skipCurrentElement();
        }
    }

    QTC_CHECK(depthOk);
    return explainingStep;
}

Analyzer::DiagnosticLocation ClangStaticAnalyzerLogFileReader::readLocationDict(bool elementIsRead)
{
    Analyzer::DiagnosticLocation location;
    if (elementIsRead) {
        QTC_ASSERT(m_xml.isStartElement() && m_xml.name() == QLatin1String("dict"),
                   return location);
    } else {
        QTC_ASSERT(m_xml.readNextStartElement() && m_xml.name() == QLatin1String("dict"),
                   return location);
    }

    int line = 0;
    int column = 0;
    int fileIndex = 0;
    bool lineOk = false, columnOk = false, fileIndexOk = false;

    // Collect values
    while (m_xml.readNextStartElement()) {
        if (m_xml.name() == QLatin1String("key")) {
            const QString keyName = m_xml.readElementText();
            if (keyName == QLatin1String("line"))
                line = readInteger(&lineOk);
            else if (keyName == QLatin1String("col"))
                column = readInteger(&columnOk);
            else if (keyName == QLatin1String("file"))
                fileIndex = readInteger(&fileIndexOk);
        } else {
            m_xml.skipCurrentElement();
        }
    }

    if (lineOk && columnOk && fileIndexOk) {
        QTC_ASSERT(fileIndex < m_referencedFiles.size(), return location);
        location = Analyzer::DiagnosticLocation(m_referencedFiles.at(fileIndex), line, column);
    }
    return location;
}

QList<Analyzer::DiagnosticLocation> ClangStaticAnalyzerLogFileReader::readRangesArray()
{
    QList<Analyzer::DiagnosticLocation> result;

    // It's an array of arrays...
    QTC_ASSERT(m_xml.readNextStartElement() && m_xml.name() == QLatin1String("array"),
               return result);
    QTC_ASSERT(m_xml.readNextStartElement() && m_xml.name() == QLatin1String("array"),
               return result);

    while (m_xml.readNextStartElement() && m_xml.name() == QLatin1String("dict"))
        result << readLocationDict(true);

    m_xml.skipCurrentElement(); // Laeve outer array
    return result;
}

QString ClangStaticAnalyzerLogFileReader::readString()
{
    if (m_xml.readNextStartElement() && m_xml.name() == QLatin1String("string"))
        return m_xml.readElementText();

    m_xml.raiseError(QObject::tr("Expected a string element."));
    return QString();
}

QStringList ClangStaticAnalyzerLogFileReader::readStringArray()
{
    if (m_xml.readNextStartElement() && m_xml.name() == QLatin1String("array")) {
        QStringList result;
        while (m_xml.readNextStartElement() && m_xml.name() == QLatin1String("string")) {
            const QString string = m_xml.readElementText();
            if (!string.isEmpty())
                result << string;
        }
        return result;
    }

    m_xml.raiseError(QObject::tr("Expected an array element."));
    return QStringList();
}

int ClangStaticAnalyzerLogFileReader::readInteger(bool *convertedSuccessfully)
{
    if (m_xml.readNextStartElement() && m_xml.name() == QLatin1String("integer")) {
        const QString contents = m_xml.readElementText();
        return contents.toInt(convertedSuccessfully);
    }

    m_xml.raiseError(QObject::tr("Expected an integer element."));
    if (convertedSuccessfully)
        *convertedSuccessfully = false;
    return -1;
}

} // namespace Internal
} // namespace ClangStaticAnalyzer