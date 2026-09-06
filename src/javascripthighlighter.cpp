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
        {Rule::Kind::Keyword,
         QRegularExpression(QStringLiteral("\\b(?:async|await|const|else|false|function|if|let|null|return|true|var)\\b")),
         keyword},
        {Rule::Kind::String,
         QRegularExpression(QStringLiteral("(?:\"(?:\\\\.|[^\"\\\\])*\"|'(?:\\\\.|[^'\\\\])*'|`(?:\\\\.|[^`\\\\])*`)")),
         string},
        {Rule::Kind::Number,
         QRegularExpression(QStringLiteral("\\b(?:0[xX][0-9A-Fa-f]+|\\d+(?:\\.\\d+)?)\\b")),
         number},
        {Rule::Kind::Comment,
         QRegularExpression(QStringLiteral("//[^\\n]*")),
         comment},
    };
    rehighlight();
}

void JavaScriptHighlighter::highlightBlock(const QString &text)
{
    // 注释规则会命中字符串内的 //（如 'http://a'），若按规则顺序简单覆盖，
    // 字符串后半段会被误染成注释色。因此 string 规则先行收集区间（m_rules
    // 中 string 在 comment 前），comment 匹配起点落在字符串区间内的跳过。
    // keyword/number 先于 string 染色、由 string 覆盖，顺序无碍。
    QList<QPair<int, int>> stringRanges;
    for (const Rule &rule : std::as_const(m_rules)) {
        auto matches = rule.pattern.globalMatch(text);
        while (matches.hasNext()) {
            const auto match = matches.next();
            if (rule.kind == Rule::Kind::Comment) {
                // comment 匹配可能起点在字符串内且延伸到行尾（'http://a' 后的真注释），
                // 不能整段跳过：只染与字符串区间不重叠的区段。
                int cursor = match.capturedStart();
                const int end = cursor + match.capturedLength();
                for (const auto &range : std::as_const(stringRanges)) {
                    const int rangeStart = range.first;
                    const int rangeEnd = range.first + range.second;
                    if (rangeEnd <= cursor || rangeStart >= end) {
                        continue;
                    }
                    if (rangeStart > cursor) {
                        setFormat(cursor, rangeStart - cursor, rule.format);
                    }
                    cursor = qMax(cursor, rangeEnd);
                }
                if (cursor < end) {
                    setFormat(cursor, end - cursor, rule.format);
                }
            } else if (rule.kind == Rule::Kind::String) {
                stringRanges.append({match.capturedStart(), match.capturedLength()});
                setFormat(match.capturedStart(), match.capturedLength(), rule.format);
            } else {
                setFormat(match.capturedStart(), match.capturedLength(), rule.format);
            }
        }
    }
}
