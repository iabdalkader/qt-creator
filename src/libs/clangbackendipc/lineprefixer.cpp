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

#include "lineprefixer.h"

namespace ClangBackEnd {

LinePrefixer::LinePrefixer(const QByteArray &prefix)
    : m_prefix(prefix)
    , m_previousIsEndingWithNewLine(true)
{}

QByteArray LinePrefixer::prefix(const QByteArray &text)
{
    QByteArray output = text;

    if (m_previousIsEndingWithNewLine)
        output.prepend(m_prefix);

    if (output.endsWith('\n')) {
        m_previousIsEndingWithNewLine = true;
        output.chop(1);
    } else {
        m_previousIsEndingWithNewLine = false;
    }

    output.replace("\n", "\n" + m_prefix);
    if (m_previousIsEndingWithNewLine)
        output.append('\n');

    return output;
}

} // namespace ClangBackEnd
