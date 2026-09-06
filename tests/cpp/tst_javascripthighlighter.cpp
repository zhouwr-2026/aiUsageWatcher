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
    void stringSlashInsideStringStaysStringColor();
};

namespace
{
// 找出覆盖 position 的高亮区段颜色；无则返回默认构造色（透明黑）。
QColor colorAt(const QTextDocument &document, int position)
{
    const auto ranges = document.firstBlock().layout()->formats();
    for (const auto &range : ranges) {
        if (position >= range.start && position < range.start + range.length) {
            return range.format.foreground().color();
        }
    }
    return {};
}
}

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

void JavaScriptHighlighterTest::stringSlashInsideStringStaysStringColor()
{
    QTextDocument document;
    JavaScriptHighlighter highlighter(&document,
                                      Qt::blue,
                                      Qt::darkGreen,
                                      Qt::darkGray,
                                      Qt::darkMagenta);
    // 字符串字面量内的 //（URL）不得被注释规则二次着色。
    document.setPlainText(QStringLiteral("const url = 'http://a.com/x'; // note"));
    highlighter.rehighlight();

    QCOMPARE(colorAt(document, 19), QColor(Qt::darkGreen)); // 'http://' 中的 '/'
    QCOMPARE(colorAt(document, 25), QColor(Qt::darkGreen)); // '.com/x' 中的 '/'
    QCOMPARE(colorAt(document, 30), QColor(Qt::darkGray));  // 行尾真注释 '//'
}

QTEST_GUILESS_MAIN(JavaScriptHighlighterTest)

#include "tst_javascripthighlighter.moc"
