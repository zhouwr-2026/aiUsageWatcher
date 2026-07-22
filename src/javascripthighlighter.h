// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <QColor>
#include <QRegularExpression>
#include <QSyntaxHighlighter>

class JavaScriptHighlighter final : public QSyntaxHighlighter
{
    Q_OBJECT

public:
    JavaScriptHighlighter(QTextDocument *document,
                          const QColor &keywordColor,
                          const QColor &stringColor,
                          const QColor &commentColor,
                          const QColor &numberColor);

    void updateColors(const QColor &keywordColor,
                      const QColor &stringColor,
                      const QColor &commentColor,
                      const QColor &numberColor);

protected:
    void highlightBlock(const QString &text) override;

private:
    struct Rule {
        QRegularExpression pattern;
        QTextCharFormat format;
    };
    QList<Rule> m_rules;
};
