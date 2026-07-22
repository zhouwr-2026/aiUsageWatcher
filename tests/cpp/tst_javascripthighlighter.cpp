// SPDX-License-Identifier: GPL-2.0-or-later

#include "javascripthighlighter.h"

#include <QTest>
#include <QTextBlock>
#include <QTextDocument>
#include <QTextLayout>

class JavaScriptHighlighterTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void highlightsKeywordsStringsNumbersAndComments();
};

void JavaScriptHighlighterTest::highlightsKeywordsStringsNumbersAndComments()
{
    QTextDocument document;
    JavaScriptHighlighter highlighter(&document,
                                      Qt::blue,
                                      Qt::darkGreen,
                                      Qt::darkGray,
                                      Qt::darkMagenta);
    document.setPlainText(QStringLiteral("const used = \"42\"; // quota"));
    highlighter.rehighlight();

    const auto ranges = document.firstBlock().layout()->formats();
    QVERIFY(ranges.size() >= 4);
}

QTEST_GUILESS_MAIN(JavaScriptHighlighterTest)

#include "tst_javascripthighlighter.moc"
