/**************************************************************************
**
** This file is part of Qt Creator
**
** Copyright (c) 2011 Nokia Corporation and/or its subsidiary(-ies).
**
** Contact: Nokia Corporation (info@qt.nokia.com)
**
**
** GNU Lesser General Public License Usage
**
** This file may be used under the terms of the GNU Lesser General Public
** License version 2.1 as published by the Free Software Foundation and
** appearing in the file LICENSE.LGPL included in the packaging of this file.
** Please review the following information to ensure the GNU Lesser General
** Public License version 2.1 requirements will be met:
** http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** In addition, as a special exception, Nokia gives you certain additional
** rights. These rights are described in the Nokia Qt LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
** Other Usage
**
** Alternatively, this file may be used in accordance with the terms and
** conditions contained in a signed written agreement between you and Nokia.
**
** If you have questions regarding the use of this file, please contact
** Nokia at info@qt.nokia.com.
**
**************************************************************************/

#include "iassistproposalitem.h"

using namespace TextEditor;

/*!
    \class IAssistProposalItem
    \brief The IAssistProposalItem is an interface for representing an assist proposal item.
*/

IAssistProposalItem::IAssistProposalItem()
{}

IAssistProposalItem::~IAssistProposalItem()
{}

/*!
    \fn bool implicitlyApplies() const

    Returns whether this item should implicitly apply in the case it is the only proposal
    item available.
*/

/*!
    \fn bool prematurelyApplies(const QChar &c) const

    Returns whether the character \a c causes this item to be applied.
*/

/*!
    \fn void apply(BaseTextEditor *editor, int basePosition) const

    This is the place to implement the actual application of the item.
*/
