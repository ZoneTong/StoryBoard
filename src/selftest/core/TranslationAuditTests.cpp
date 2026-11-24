#include "selftest/core/SelfTestContext.h"
#include "selftest/core/SelftestEntries.h"

#include <QCoreApplication>
#include <QFile>
#include <QStringList>
#include <QXmlStreamReader>

// 翻译审计：把打进资源里的 storyboard_zh.ts 逐条解析出来，再用运行时同一个
// QCoreApplication::translate() 反查。任何「有译文但上下文对不上」的条目都会
// 在这里现形——那正是面板 tooltip 显示英文的那类问题。
void selftest::RunTranslationAuditTests(Context& ctx)
{
    QFile file(QStringLiteral(":/i18n/storyboard_zh.ts"));
    const bool opened = file.open(QIODevice::ReadOnly | QIODevice::Text);
    ctx.Check(opened, QStringLiteral("内嵌的资源里能找到 storyboard_zh.ts"));

    int messageCount = 0;
    int checkedCount = 0;
    int numerusCount = 0;
    int emptyCount = 0;
    int unfinishedCount = 0;
    QStringList emptySamples;
    QStringList unfinishedSamples;
    QStringList mismatched;
    QStringList mismatchedSamples;

    if (opened) {
        QXmlStreamReader xml(&file);
        QString currentContext;
        QString currentSource;
        bool inMessage = false;
        bool numerus = false;
        while (!xml.atEnd()) {
            xml.readNext();
            if (xml.isStartElement()) {
                const auto element = xml.name();
                if (element == QLatin1String("name") && !inMessage) {
                    currentContext = xml.readElementText();
                } else if (element == QLatin1String("message")) {
                    inMessage = true;
                    numerus = xml.attributes().value(QLatin1String("numerus")) == QLatin1String("yes");
                    currentSource.clear();
                    if (!numerus) {
                        ++messageCount;
                    } else {
                        ++numerusCount;
                    }
                } else if (inMessage && element == QLatin1String("source")) {
                    currentSource = xml.readElementText();
                } else if (inMessage && element == QLatin1String("translation")) {
                    // 复数条目的 <translation> 里是若干 <numerusform> 子元素，不能当纯文本读，
                    // 否则解析器会在第一个 numerus 条目上报「需要字符数据」。
                    if (numerus) {
                        xml.skipCurrentElement();
                        continue;
                    }
                    const QString type = xml.attributes().value(QLatin1String("type")).toString();
                    const QString translation = xml.readElementText();
                    if (type.startsWith(QLatin1String("unfinished")) || type.startsWith(QLatin1String("vanished"))) {
                        ++unfinishedCount;
                        if (unfinishedSamples.size() < 5) {
                            unfinishedSamples << QStringLiteral("%1 · %2").arg(currentContext, currentSource);
                        }
                        continue;
                    }
                    if (translation.isEmpty()) {
                        ++emptyCount;
                        if (emptySamples.size() < 5) {
                            emptySamples << QStringLiteral("%1 · %2").arg(currentContext, currentSource);
                        }
                        continue;
                    }
                    ++checkedCount;
                    const QString resolved = QCoreApplication::translate(currentContext.toUtf8().constData(),
                                                                         currentSource.toUtf8().constData());
                    if (resolved != translation) {
                        mismatched << QStringLiteral("%1 · %2").arg(currentContext, currentSource);
                        if (mismatchedSamples.size() < 5) {
                            mismatchedSamples << QStringLiteral("%1 · %2").arg(currentContext, currentSource);
                        }
                    }
                }
            } else if (xml.isEndElement() && xml.name() == QLatin1String("message")) {
                inMessage = false;
            }
        }
        ctx.Check(!xml.hasError(), QStringLiteral("storyboard_zh.ts 是良构 XML"), xml.errorString());
    }

    ctx.Check(messageCount > 1000, QStringLiteral("翻译表条目数量正常"), QString::number(messageCount));
    ctx.Check(emptyCount == 0, QStringLiteral("翻译表没有空译文"), emptySamples.join(QStringLiteral(" / ")));
    ctx.Check(unfinishedCount == 0, QStringLiteral("翻译表没有 unfinished / vanished 条目"),
              unfinishedSamples.join(QStringLiteral(" / ")));
    ctx.Check(
        checkedCount == messageCount, QStringLiteral("每条译文都被扫描到（复数条目单独计数）"),
        QStringLiteral("checked=%1 messages=%2 numerus=%3").arg(checkedCount).arg(messageCount).arg(numerusCount));
    ctx.Check(mismatched.isEmpty(), QStringLiteral("所有译文都能被运行时按上下文取到"),
              mismatched.isEmpty() ? QString()
                                   : QStringLiteral("%1 条不一致，例：%2")
                                         .arg(mismatched.size())
                                         .arg(mismatchedSamples.join(QStringLiteral(" / "))));

    // 具体回归点：编辑面板上的箭头图标提示曾因上下文写错而显示英文。
    ctx.Check(QCoreApplication::translate("PropertiesPanel", "Arrow heads") == QStringLiteral("箭头样式"),
              QStringLiteral("编辑面板箭头提示有中文译文"),
              QCoreApplication::translate("PropertiesPanel", "Arrow heads"));
}
