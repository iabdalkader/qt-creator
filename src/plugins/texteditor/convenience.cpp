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

#include "convenience.h"

#include <QTextDocument>
#include <QTextBlock>
#include <QTextCursor>

namespace TextEditor {
namespace Convenience {

bool convertPosition(const QTextDocument *document, int pos, int *line, int *column)
{
    QTextBlock block = document->findBlock(pos);
    if (!block.isValid()) {
        (*line) = -1;
        (*column) = -1;
        return false;
    } else {
        (*line) = block.blockNumber() + 1;
        (*column) = pos - block.position();
        return true;
    }
}

QString textAt(QTextCursor tc, int pos, int length)
{
    if (pos < 0)
        pos = 0;
    tc.movePosition(QTextCursor::End);
    if (pos + length > tc.position())
        length = tc.position() - pos;

    tc.setPosition(pos);
    tc.setPosition(pos + length, QTextCursor::KeepAnchor);

    // selectedText() returns U+2029 (PARAGRAPH SEPARATOR) instead of newline
    return tc.selectedText().replace(QChar::ParagraphSeparator, QLatin1Char('\n'));
}

QTextCursor flippedCursor(const QTextCursor &cursor)
{
    QTextCursor flipped = cursor;
    flipped.clearSelection();
    flipped.setPosition(cursor.anchor(), QTextCursor::KeepAnchor);
    return flipped;
}

} // Util
} // TextEditor
