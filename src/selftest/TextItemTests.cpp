#include "Alignment.h"
#include "BoolOps.h"
#include "CanvasScene.h"
#include "CanvasTypes.h"
#include "CanvasView.h"
#include "Commands.h"
#include "CompositeShapeItem.h"
#include "CurveItem.h"
#include "CustomShapeTemplates.h"
#include "ExportDialog.h"
#include "I18n.h"
#include "ItemDockPanel.h"
#include "ItemPropertiesDialog.h"
#include "LayerPanel.h"
#include "LineArtConverter.h"
#include "LineArtDialog.h"
#include "LineArtPresets.h"
#include "MainWindow.h"
#include "Settings.h"
#include "SettingsDialog.h"
#include "ShapeItem.h"
#include "TextItem.h"
#include "selftest/SelfTestContext.h"
#include "selftest/SelftestEntries.h"
#include "selftest/SelftestHelpers.h"

#include <QAction>
#include <QApplication>
#include <QBuffer>
#include <QCheckBox>
#include <QComboBox>
#include <QContextMenuEvent>
#include <QCoreApplication>
#include <QDialogButtonBox>
#include <QDir>
#include <QDockWidget>
#include <QDoubleSpinBox>
#include <QFile>
#include <QFileInfo>
#include <QFont>
#include <QGroupBox>
#include <QImage>
#include <QImageWriter>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QLineF>
#include <QLinearGradient>
#include <QListWidget>
#include <QListWidgetItem>
#include <QMarginsF>
#include <QMenu>
#include <QMenuBar>
#include <QMouseEvent>
#include <QPageLayout>
#include <QPageSize>
#include <QPainter>
#include <QPainterPath>
#include <QPdfWriter>
#include <QPixmap>
#include <QPlainTextEdit>
#include <QPolygonF>
#include <QPushButton>
#include <QScrollArea>
#include <QScrollBar>
#include <QSet>
#include <QSize>
#include <QSpinBox>
#include <QStandardPaths>
#include <QStringConverter>
#include <QTextStream>
#include <QTimer>
#include <QToolBar>
#include <QToolButton>
#include <QUndoStack>
#include <QWheelEvent>
#include <QWidget>
#include <QWidgetAction>
#include <QtMath>
#include <cstdlib>


void selftest::runTextItemTests(Context& ctx)
{
    // Text items and the per-layer unique-height invariant.
    // ---------------------------------------------------------------
    CanvasScene textScene;
    QUndoStack textUndo;
    textUndo.setUndoLimit(100);
    textScene.setUndoStack(&textUndo);
    textScene.applyCanvasSize(QSizeF(900, 600));
    textScene.setShowGrid(false);

    QFont headingFont(QStringLiteral("Microsoft YaHei UI"));
    headingFont.setPointSize(36);
    headingFont.setBold(true);
    auto* heading = new TextItem();
    heading->setTextFormat(QStringLiteral("第一行标题\n第二行文字"), headingFont, QColor(QStringLiteral("#1D4ED8")));
    heading->setPos(QPointF(80, 70));
    const QSizeF headingNatural = heading->naturalSize();
    textScene.addItemsWithUndo({heading}, QStringLiteral("添加文字"));

    ctx.check(heading->kind() == QStringLiteral("text") && textScene.canvasItemCount() == 1,
              QStringLiteral("文字图元加入场景"));
    ctx.check(heading->text() == QStringLiteral("第一行标题\n第二行文字"), QStringLiteral("文字内容保存"));
    ctx.check(heading->textFont().bold() && qAbs(heading->textFont().pointSizeF() - 36.0) < 0.1,
              QStringLiteral("文字字体样式保存"));
    ctx.check(heading->textColor() == QColor(QStringLiteral("#1D4ED8")), QStringLiteral("文字颜色保存"));
    ctx.check(heading->contentSize().width() >= headingNatural.width() - 0.5 &&
                  heading->contentSize().height() >= headingNatural.height() - 0.5,
              QStringLiteral("文字图元按内容计算尺寸"), geometryText(heading->pos(), heading->contentSize()));
    ctx.check(heading->layerId() == textScene.activeLayerId() && heading->height() == 1,
              QStringLiteral("文字进入当前图层并从 height 1 开始"));
    ctx.check(textScene.layerHeightsUnique(textScene.activeLayerId()), QStringLiteral("文字加入后层内 height 唯一"));

    const QImage textRendered = textScene.renderImage(textScene.canvasRect(), 1.0);
    const QRect textArea = heading->sceneContentRect().toAlignedRect().intersected(textRendered.rect());
    int textInkPixels = 0;
    for (int y = textArea.top(); y <= textArea.bottom(); ++y) {
        for (int x = textArea.left(); x <= textArea.right(); ++x) {
            const QColor textPixel = textRendered.pixelColor(x, y);
            if (textPixel.alpha() > 0 && textPixel.lightness() < 240) {
                ++textInkPixels;
            }
        }
    }
    ctx.check(textInkPixels > 40, QStringLiteral("文字图元实际渲染到画板"),
              QStringLiteral("ink=%1").arg(textInkPixels));

    textUndo.undo();
    ctx.check(textScene.canvasItemCount() == 0, QStringLiteral("撤销添加文字"));
    textUndo.redo();
    ctx.check(textScene.canvasItemCount() == 1, QStringLiteral("重做添加文字"));

    QFont editedFont(QStringLiteral("Microsoft YaHei UI"));
    editedFont.setPointSize(24);
    editedFont.setItalic(true);
    editedFont.setUnderline(true);
    editedFont.setStrikeOut(true);
    ctx.check(textScene.changeTextWithUndo(heading, QStringLiteral("更新后的文字"), editedFont,
                                           QColor(QStringLiteral("#B91C1C")), QStringLiteral("编辑文字")),
              QStringLiteral("编辑文字进入撤销栈"));
    ctx.check(heading->text() == QStringLiteral("更新后的文字") && heading->textFont().italic() &&
                  heading->textFont().underline() && heading->textFont().strikeOut() &&
                  heading->textColor() == QColor(QStringLiteral("#B91C1C")),
              QStringLiteral("编辑文字内容与样式生效"));
    textUndo.undo();
    ctx.check(heading->text() == QStringLiteral("第一行标题\n第二行文字") && heading->textFont().bold() &&
                  heading->textColor() == QColor(QStringLiteral("#1D4ED8")),
              QStringLiteral("撤销编辑文字"));
    textUndo.redo();
    ctx.check(heading->text() == QStringLiteral("更新后的文字"), QStringLiteral("重做编辑文字"));

    auto* secondText = new TextItem();
    QFont bodyFont(QStringLiteral("Microsoft YaHei UI"));
    bodyFont.setPointSize(22);
    secondText->setTextFormat(QStringLiteral("第二段文字"), bodyFont, QColor(QStringLiteral("#0F766E")));
    secondText->setPos(QPointF(120, 260));
    textScene.addItemsWithUndo({secondText}, QStringLiteral("添加第二段文字"));
    ctx.check(secondText->height() == 2 && textScene.layerHeightsUnique(textScene.activeLayerId()),
              QStringLiteral("同层文字高度继续递增且唯一"));

    ctx.check(textScene.adjustItemHeight(secondText, -1), QStringLiteral("文字图元支持高度 -1"));
    ctx.check(secondText->height() == 1 && heading->height() == 2 &&
                  textScene.layerHeightsUnique(textScene.activeLayerId()),
              QStringLiteral("文字高度交换后仍然唯一"));
    textUndo.undo();
    ctx.check(secondText->height() == 2 && heading->height() == 1, QStringLiteral("撤销文字高度交换"));

    const QJsonObject textRoot = textScene.toJson();
    ctx.check(textRoot.value(QStringLiteral("version")).toInt() == 2 &&
                  QJsonDocument(textRoot).toJson(QJsonDocument::Compact).contains("\"text\""),
              QStringLiteral("画布保存文字图元"));
    CanvasScene textLoaded;
    QString textLoadError;
    ctx.check(textLoaded.fromJson(textRoot, &textLoadError), QStringLiteral("画布反序列化文字图元"), textLoadError);
    TextItem* loadedText = nullptr;
    for (CanvasItem* item : textLoaded.canvasItems()) {
        if (auto* text = dynamic_cast<TextItem*>(item)) {
            loadedText = text;
            break;
        }
    }
    ctx.check(loadedText != nullptr, QStringLiteral("画布恢复文字类型"));
    if (loadedText) {
        ctx.check(loadedText->text() == QStringLiteral("更新后的文字") && loadedText->textFont().italic() &&
                      loadedText->textFont().underline() && loadedText->textFont().strikeOut() &&
                      loadedText->textColor() == QColor(QStringLiteral("#B91C1C")),
                  QStringLiteral("画布恢复文字内容与样式"));
        ctx.check(loadedText->layerId() == textScene.activeLayerId() && loadedText->height() == heading->height(),
                  QStringLiteral("画布恢复文字图层与高度"));
        ctx.check(qAbs(loadedText->contentSize().width() - heading->contentSize().width()) < 0.5 &&
                      qAbs(loadedText->contentSize().height() - heading->contentSize().height()) < 0.5,
                  QStringLiteral("画布恢复文字尺寸"));
    }
    ctx.check(textLoaded.layerHeightsUnique(textLoaded.activeLayerId()), QStringLiteral("画布载入后层内 height 唯一"));

    // A hand-edited canvas may contain duplicate heights. Loading it must
    // repair them to a stable, unique sequence while preserving item order.
    QJsonObject duplicateRoot = textScene.toJson();
    QJsonArray duplicateItems = duplicateRoot.value(QStringLiteral("items")).toArray();
    for (int i = 0; i < duplicateItems.size(); ++i) {
        QJsonObject itemJson = duplicateItems.at(i).toObject();
        itemJson.insert(QStringLiteral("height"), 1);
        duplicateItems.replace(i, itemJson);
    }
    duplicateRoot.insert(QStringLiteral("items"), duplicateItems);
    CanvasScene repairedScene;
    QString repairedError;
    ctx.check(repairedScene.fromJson(duplicateRoot, &repairedError), QStringLiteral("载入含重复 height 的画布"),
              repairedError);
    ctx.check(repairedScene.layerHeightsUnique(repairedScene.activeLayerId()) && repairedScene.canvasItemCount() == 2,
              QStringLiteral("重复 height 载入后自动修复为唯一值"));
    QSet<int> repairedHeights;
    for (CanvasItem* item : repairedScene.canvasItems()) {
        repairedHeights.insert(item->height());
    }
    ctx.check(repairedHeights == QSet<int>({1, 2}), QStringLiteral("修复后的 height 序列连续且唯一"));

    QJsonObject oversizedRoot = textScene.toJson();
    QJsonArray oversizedItems = oversizedRoot.value(QStringLiteral("items")).toArray();
    for (int i = 0; i < oversizedItems.size(); ++i) {
        QJsonObject itemJson = oversizedItems.at(i).toObject();
        itemJson.insert(QStringLiteral("height"), 1000000 + i);
        oversizedItems.replace(i, itemJson);
    }
    oversizedRoot.insert(QStringLiteral("items"), oversizedItems);
    CanvasScene oversizedScene;
    QString oversizedError;
    ctx.check(oversizedScene.fromJson(oversizedRoot, &oversizedError), QStringLiteral("载入超出高度上限的画布"),
              oversizedError);
    bool oversizedRepaired = oversizedScene.layerHeightsUnique(oversizedScene.activeLayerId());
    for (CanvasItem* item : oversizedScene.canvasItems()) {
        oversizedRepaired = oversizedRepaired && item->height() <= 999999;
    }
    ctx.check(oversizedRepaired, QStringLiteral("超限 height 载入后修复到合法唯一范围"));

    // ---------------------------------------------------------------
}
