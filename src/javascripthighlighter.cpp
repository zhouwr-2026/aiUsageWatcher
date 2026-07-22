// SPDX-License-Identifier: GPL-2.0-or-later

#include "javascripthighlighter.h"

#include <QTextDocument>

JavaScriptHighlighter::JavaScriptHighlighter(QTextDocument *document,
                                             const QColor &keywordColor,
                                             const QColor &stringColor,
                                             const QColor &commentColor,
                                             const QColor &numberColor)
    : QSyntaxHighlighter(document)
{
    setObjectName(QStringLiteral("quotaPilotJavaScriptHighlighter"));
    updateColors(keywordColor, stringColor, commentColor, numberColor);
}

void JavaScriptHighlighter::updateColors(const QColor &keywordColor,
                                         const QColor &stringColor,
                                         const QColor &commentColor,
                                         const QColor &numberColor)
{
    QTextCharFormat keyword;
    keyword.setForeground(keywordColor);
    keyword.setFontWeight(QFont::DemiBold);
    QTextCharFormat string;
    string.setForeground(stringColor);
    QTextCharFormat comment;
    comment.setForeground(commentColor);
    comment.setFontItalic(true);
    QTextCharFormat number;
    number.setForeground(numberColor);

    m_rules = {
        {QRegularExpression(QStringLiteral("\\b(?:async|await|const|else|false|function|if|let|null|return|true|var)\\b")), keyword},
        {QRegularExpression(QStringLiteral("(?:\"(?:\\\\.|[^\"\\\\])*\"|'(?:\\\\.|[^'\\\\])*'|`(?:\\\\.|[^`\\\\])*`)")), string},
        {QRegularExpression(QStringLiteral("\\b(?:0[xX][0-9A-Fa-f]+|\\d+(?:\\.\\d+)?)\\b")), number},
        {QRegularExpression(QStringLiteral("//[^\\n]*")), comment},
    };
    rehighlight();
}

void JavaScriptHighlighter::highlightBlock(const QString &text)
{
    for (const Rule &rule : std::as_const(m_rules)) {
        auto matches = rule.pattern.globalMatch(text);
        while (matches.hasNext()) {
            const auto match = matches.next();
            setFormat(match.capturedStart(), match.capturedLength(), rule.format);
        }
    }
}
