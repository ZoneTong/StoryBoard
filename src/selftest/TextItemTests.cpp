#include "Alignment.h"
#include "BoolOps.h"
#include "CanvasScene.h"
#include "CanvasTypes.h"
#include "CanvasView.h"
#include "Commands.h"
#include "CurveItem.h"
#include "CustomShapeTemplates.h"
#include "ExportDialog.h"
#include "I18n.h"
#include "ItemPropertiesDialog.h"
#include "LayerPanel.h"
#include "LineArtConverter.h"
#include "LineArtDialog.h"
#include "LineArtPresets.h"
#include "MainWindow.h"
#include "PolygonItem.h"
#include "PropertiesPanel.h"
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


void selftest::RunTextItemTests(Context& ctx)
{
    // Text items and the per-layer unique-height invariant.
    // ---------------------------------------------------------------
    CanvasScene textScene;
    QUndoStack textUndo;
    textUndo.setUndoLimit(100);
    textScene.SetUndoStack(&textUndo);
    textScene.ApplyCanvasSize(QSizeF(900, 600));
    textScene.SetShowGrid(false);

    QFont headingFont(QStringLiteral("Microsoft YaHei UI"));
    headingFont.setPointSize(36);
    headingFont.setBold(true);
    auto* heading = new TextItem();
    heading->SetTextFormat(QStringLiteral("第一行标题\n第二行文字"), headingFont, QColor(QStringLiteral("#1D4ED8")));
    heading->setPos(QPointF(80, 70));
    const QSizeF headingNatural = heading->NaturalSize();
    textScene.AddItemsWithUndo({heading}, QStringLiteral("添加文字"));

    ctx.Check(heading->Kind() == QStringLiteral("text") && textScene.CanvasItemCount() == 1,
              QStringLiteral("文字图元加入场景"));
    ctx.Check(heading->Text() == QStringLiteral("第一行标题\n第二行文字"), QStringLiteral("文字内容保存"));
    ctx.Check(heading->TextFont().bold() && qAbs(heading->TextFont().pointSizeF() - 36.0) < 0.1,
              QStringLiteral("文字字体样式保存"));
    ctx.Check(heading->TextColor() == QColor(QStringLiteral("#1D4ED8")), QStringLiteral("文字颜色保存"));
    TextAttributes* textAttrs = dynamic_cast<TextAttributes*>(heading);
    const TextAttributesState textState = textAttrs ? textAttrs->TextAttributeState() : TextAttributesState();
    ctx.Check(textAttrs != nullptr && textState.text == heading->Text() &&
                  qAbs(textState.baseSize.width() - headingNatural.width()) < 0.5,
              QStringLiteral("文字状态经由 TextAttributes 基类读写"));
    ctx.Check(heading->ContentSize().width() >= headingNatural.width() - 0.5 &&
                  heading->ContentSize().height() >= headingNatural.height() - 0.5,
              QStringLiteral("文字图元按内容计算尺寸"), GeometryText(heading->pos(), heading->ContentSize()));
    ctx.Check(heading->LayerId() == textScene.ActiveLayerId() && heading->ZOrder() == 1,
              QStringLiteral("文字进入当前图层并从 height 1 开始"));
    ctx.Check(textScene.LayerHeightsUnique(textScene.ActiveLayerId()), QStringLiteral("文字加入后层内 height 唯一"));

    const QImage textRendered = textScene.RenderImage(textScene.CanvasRect(), 1.0);
    const QRect textArea = heading->SceneContentRect().toAlignedRect().intersected(textRendered.rect());
    int textInkPixels = 0;
    for (int y = textArea.top(); y <= textArea.bottom(); ++y) {
        for (int x = textArea.left(); x <= textArea.right(); ++x) {
            const QColor textPixel = textRendered.pixelColor(x, y);
            if (textPixel.alpha() > 0 && textPixel.lightness() < 240) {
                ++textInkPixels;
            }
        }
    }
    ctx.Check(textInkPixels > 40, QStringLiteral("文字图元实际渲染到画板"),
              QStringLiteral("ink=%1").arg(textInkPixels));

    textUndo.undo();
    ctx.Check(textScene.CanvasItemCount() == 0, QStringLiteral("撤销添加文字"));
    textUndo.redo();
    ctx.Check(textScene.CanvasItemCount() == 1, QStringLiteral("重做添加文字"));

    QFont editedFont(QStringLiteral("Microsoft YaHei UI"));
    editedFont.setPointSize(24);
    editedFont.setItalic(true);
    editedFont.setUnderline(true);
    editedFont.setStrikeOut(true);
    ctx.Check(textScene.ChangeTextWithUndo(heading, QStringLiteral("更新后的文字"), editedFont,
                                           QColor(QStringLiteral("#B91C1C")), QStringLiteral("编辑文字")),
              QStringLiteral("编辑文字进入撤销栈"));
    ctx.Check(heading->Text() == QStringLiteral("更新后的文字") && heading->TextFont().italic() &&
                  heading->TextFont().underline() && heading->TextFont().strikeOut() &&
                  heading->TextColor() == QColor(QStringLiteral("#B91C1C")),
              QStringLiteral("编辑文字内容与样式生效"));
    textUndo.undo();
    ctx.Check(heading->Text() == QStringLiteral("第一行标题\n第二行文字") && heading->TextFont().bold() &&
                  heading->TextColor() == QColor(QStringLiteral("#1D4ED8")),
              QStringLiteral("撤销编辑文字"));
    textUndo.redo();
    ctx.Check(heading->Text() == QStringLiteral("更新后的文字"), QStringLiteral("重做编辑文字"));

    auto* secondText = new TextItem();
    QFont bodyFont(QStringLiteral("Microsoft YaHei UI"));
    bodyFont.setPointSize(22);
    secondText->SetTextFormat(QStringLiteral("第二段文字"), bodyFont, QColor(QStringLiteral("#0F766E")));
    secondText->setPos(QPointF(120, 260));
    textScene.AddItemsWithUndo({secondText}, QStringLiteral("添加第二段文字"));
    ctx.Check(secondText->ZOrder() == 2 && textScene.LayerHeightsUnique(textScene.ActiveLayerId()),
              QStringLiteral("同层文字高度继续递增且唯一"));

    ctx.Check(textScene.AdjustItemHeight(secondText, -1), QStringLiteral("文字图元支持高度 -1"));
    ctx.Check(secondText->ZOrder() == 1 && heading->ZOrder() == 2 &&
                  textScene.LayerHeightsUnique(textScene.ActiveLayerId()),
              QStringLiteral("文字高度交换后仍然唯一"));
    textUndo.undo();
    ctx.Check(secondText->ZOrder() == 2 && heading->ZOrder() == 1, QStringLiteral("撤销文字高度交换"));

    const QJsonObject textRoot = textScene.ToJson();
    ctx.Check(textRoot.value(QStringLiteral("version")).toInt() == 2 &&
                  QJsonDocument(textRoot).toJson(QJsonDocument::Compact).contains("\"text\""),
              QStringLiteral("画布保存文字图元"));
    CanvasScene textLoaded;
    QString textLoadError;
    ctx.Check(textLoaded.FromJson(textRoot, &textLoadError), QStringLiteral("画布反序列化文字图元"), textLoadError);
    TextItem* loadedText = nullptr;
    for (CanvasItem* item : textLoaded.CanvasItems()) {
        if (auto* text = dynamic_cast<TextItem*>(item)) {
            loadedText = text;
            break;
        }
    }
    ctx.Check(loadedText != nullptr, QStringLiteral("画布恢复文字类型"));
    if (loadedText) {
        ctx.Check(loadedText->Text() == QStringLiteral("更新后的文字") && loadedText->TextFont().italic() &&
                      loadedText->TextFont().underline() && loadedText->TextFont().strikeOut() &&
                      loadedText->TextColor() == QColor(QStringLiteral("#B91C1C")),
                  QStringLiteral("画布恢复文字内容与样式"));
        ctx.Check(loadedText->LayerId() == textScene.ActiveLayerId() && loadedText->ZOrder() == heading->ZOrder(),
                  QStringLiteral("画布恢复文字图层与高度"));
        ctx.Check(qAbs(loadedText->ContentSize().width() - heading->ContentSize().width()) < 0.5 &&
                      qAbs(loadedText->ContentSize().height() - heading->ContentSize().height()) < 0.5,
                  QStringLiteral("画布恢复文字尺寸"));
    }
    ctx.Check(textLoaded.LayerHeightsUnique(textLoaded.ActiveLayerId()), QStringLiteral("画布载入后层内 height 唯一"));

    // A hand-edited canvas may contain duplicate heights. Loading it must
    // repair them to a stable, unique sequence while preserving item order.
    QJsonObject duplicateRoot = textScene.ToJson();
    QJsonArray duplicateItems = duplicateRoot.value(QStringLiteral("items")).toArray();
    for (int i = 0; i < duplicateItems.size(); ++i) {
        QJsonObject itemJson = duplicateItems.at(i).toObject();
        itemJson.insert(QStringLiteral("zOrder"), 1);
        duplicateItems.replace(i, itemJson);
    }
    duplicateRoot.insert(QStringLiteral("items"), duplicateItems);
    CanvasScene repairedScene;
    QString repairedError;
    ctx.Check(repairedScene.FromJson(duplicateRoot, &repairedError), QStringLiteral("载入含重复 height 的画布"),
              repairedError);
    ctx.Check(repairedScene.LayerHeightsUnique(repairedScene.ActiveLayerId()) && repairedScene.CanvasItemCount() == 2,
              QStringLiteral("重复 height 载入后自动修复为唯一值"));
    QSet<int> repairedHeights;
    for (CanvasItem* item : repairedScene.CanvasItems()) {
        repairedHeights.insert(item->ZOrder());
    }
    ctx.Check(repairedHeights == QSet<int>({1, 2}), QStringLiteral("修复后的 height 序列连续且唯一"));

    QJsonObject oversizedRoot = textScene.ToJson();
    QJsonArray oversizedItems = oversizedRoot.value(QStringLiteral("items")).toArray();
    for (int i = 0; i < oversizedItems.size(); ++i) {
        QJsonObject itemJson = oversizedItems.at(i).toObject();
        itemJson.insert(QStringLiteral("zOrder"), 1000000 + i);
        oversizedItems.replace(i, itemJson);
    }
    oversizedRoot.insert(QStringLiteral("items"), oversizedItems);
    CanvasScene oversizedScene;
    QString oversizedError;
    ctx.Check(oversizedScene.FromJson(oversizedRoot, &oversizedError), QStringLiteral("载入超出高度上限的画布"),
              oversizedError);
    bool oversizedRepaired = oversizedScene.LayerHeightsUnique(oversizedScene.ActiveLayerId());
    for (CanvasItem* item : oversizedScene.CanvasItems()) {
        oversizedRepaired = oversizedRepaired && item->ZOrder() <= 999999;
    }
    ctx.Check(oversizedRepaired, QStringLiteral("超限 height 载入后修复到合法唯一范围"));

    // ---------------------------------------------------------------
}
