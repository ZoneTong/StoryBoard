#include "CanvasScene.h"
#include "CanvasView.h"
#include "I18n.h"
#include "ImageItem.h"
#include "ItemPropertiesDialog.h"
#include "ShapeItem.h"
#include "selftest/SelfTestContext.h"
#include "selftest/SelftestEntries.h"

#include <QApplication>
#include <QColor>
#include <QDir>
#include <QFile>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QPixmap>
#include <QPointF>
#include <QPushButton>
#include <QRectF>
#include <QSizeF>
#include <QUndoStack>


void selftest::runPropertyDialogImageTests(Context& ctx)
{
    CanvasScene& editScene = ctx.propertyScene;
    QUndoStack& editUndo = ctx.propertyUndo;
    ShapeItem* firstShape = ctx.propertyFirstShape;

    ctx.check(qFuzzyCompare(CanvasItem::snapRotation(12.4), 10.0) &&
                  qFuzzyCompare(CanvasItem::snapRotation(-12.4), -10.0),
              QStringLiteral("旋转手柄角度按 5° 步长吸附"));
    const QRectF rotationFrame = firstShape->contentRect();
    const QPointF rotationHandlePoint(rotationFrame.center().x(), rotationFrame.top() - 25.0);
    editScene.clearSelection();
    firstShape->setSelected(true);
    ctx.check(firstShape->shape().contains(rotationHandlePoint), QStringLiteral("选中单个图元时显示旋转手柄"));
    firstShape->setSelected(false);
    ctx.check(!firstShape->shape().contains(rotationHandlePoint), QStringLiteral("未选中图元时不显示旋转手柄"));
    firstShape->setSelected(true);
    const int rotateUndoBefore = editUndo.count();
    const int rotatedCount = editScene.rotateSelected(45.0);
    ctx.check(rotatedCount == 1 && qFuzzyCompare(firstShape->rotationAngle(), 75.0) &&
                  editUndo.count() == rotateUndoBefore + 1,
              QStringLiteral("旋转快捷键对选中图元增加 45°"));
    editUndo.undo();
    ctx.check(qFuzzyCompare(firstShape->rotationAngle(), 30.0), QStringLiteral("旋转快捷键可整体撤销"));
    editUndo.redo();
    ctx.check(qFuzzyCompare(firstShape->rotationAngle(), 75.0), QStringLiteral("旋转快捷键可重做"));
    ctx.check(editScene.rotateSelected(-45.0) == 1 && qFuzzyCompare(firstShape->rotationAngle(), 30.0),
              QStringLiteral("逆时针旋转快捷键减少 45°"));

    // 需求 35：低缩放下旋转手柄必须仍在 boundingRect 与场景命中范围内。
    {
        CanvasView lowZoomView(&editScene);
        lowZoomView.resize(900, 700);
        lowZoomView.show();
        QApplication::processEvents();
        for (int i = 0; i < 5; ++i) {
            lowZoomView.zoomOut();
        }
        const qreal lowZoom = lowZoomView.zoom();
        ctx.check(lowZoom < 0.6, QStringLiteral("低缩放测试视图确实缩小"));
        firstShape->setSelected(true);
        const QRectF lowZoomFrame = firstShape->contentRect();
        const qreal lowZoomHandleSize = qBound(6.0, 9.0 / lowZoom, 40.0);
        const QPointF lowZoomHandle(lowZoomFrame.center().x(),
                                    lowZoomFrame.top() - (lowZoomHandleSize + 16.0 / lowZoom));
        ctx.check(firstShape->boundingRect().contains(lowZoomHandle),
                  QStringLiteral("低缩放下旋转手柄位于 boundingRect 内"));
        ctx.check(firstShape->shape().contains(lowZoomHandle), QStringLiteral("低缩放下旋转手柄仍在 shape 命中范围内"));
        ctx.check(editScene.items(firstShape->mapToScene(lowZoomHandle)).contains(firstShape),
                  QStringLiteral("低缩放下旋转手柄可被场景命中"));
        lowZoomView.hide();
    }
    editScene.clearSelection();

    QJsonObject namedJson = firstShape->toJson();
    ShapeItem loadedNamedShape;
    loadedNamedShape.fromJson(namedJson);
    ctx.check(loadedNamedShape.rawName() == QStringLiteral("Hero panel"), QStringLiteral("图元名称持久化到画布"));
    ctx.check(qFuzzyCompare(loadedNamedShape.rotationAngle(), 30.0), QStringLiteral("图元旋转角度持久化到画布"));

    const int noopUndoBefore = editUndo.count();
    {
        ItemPropertiesDialog dialog(&editScene, firstShape);
        dialog.show();
        QApplication::processEvents();
        dialog.accept();
        QApplication::processEvents();
    }
    ctx.check(editUndo.count() == noopUndoBefore, QStringLiteral("属性窗未修改时确定不产生撤销记录"));

    auto* imageItem = new ImageItem();
    imageItem->setImage(QPixmap(24, 24), QStringLiteral("old.png"));
    imageItem->setContentSize(QSizeF(80.0, 80.0));
    imageItem->setPos(QPointF(400.0, 200.0));
    editScene.addItemsWithUndo({imageItem}, QStringLiteral("Add property image"));
    ctx.check(imageItem->rawName() == QStringLiteral("图片 1"), QStringLiteral("图片图元自动名称不使用文件名"),
              imageItem->rawName());
    const int imageUndoBefore = editUndo.count();
    {
        ItemPropertiesDialog dialog(&editScene, imageItem);
        dialog.show();
        QApplication::processEvents();
        QLineEdit* imageName = dialog.findChild<QLineEdit*>(QStringLiteral("ItemPropertiesImageValue"));
        QLabel* typeValue = dialog.findChild<QLabel*>(QStringLiteral("ItemPropertiesTypeValue"));
        QLabel* heading = dialog.findChild<QLabel*>(QStringLiteral("ItemPropertiesHeading"));
        ctx.check(imageName && imageName->isEnabled() && imageName->text() == QStringLiteral("old.png"),
                  QStringLiteral("图片属性窗图片名称可编辑"));
        ctx.check(typeValue && typeValue->text() == uiText(QStringLiteral("Image")) && heading &&
                      heading->text() == imageItem->name(),
                  QStringLiteral("图片属性窗标题和类型使用图元名称与类型名"));
        if (imageName) {
            imageName->setText(QStringLiteral("new.png"));
        }
        dialog.accept();
        QApplication::processEvents();
    }
    ctx.check(imageItem->imageName() == QStringLiteral("new.png") && editUndo.count() == imageUndoBefore + 1,
              QStringLiteral("图片属性窗修改图片名称并生成一条撤销记录"));
    editUndo.undo();
    QApplication::processEvents();
    ctx.check(imageItem->imageName() == QStringLiteral("old.png"), QStringLiteral("图片属性窗修改可撤销"));

    const int imageBodyUndoIndex = editUndo.index();
    {
        ItemPropertiesDialog dialog(&editScene, imageItem);
        dialog.show();
        QApplication::processEvents();
        QPushButton* replaceButton = dialog.findChild<QPushButton*>(QStringLiteral("ItemPropertiesReplaceImageValue"));
        ctx.check(replaceButton && replaceButton->isEnabled() && dialog.isModal(),
                  QStringLiteral("图片属性窗提供替换图片按钮且属性窗为模态"));
        QFile iconFile(QStringLiteral(":/icons/app.png"));
        const bool iconRead = iconFile.open(QIODevice::ReadOnly);
        const QByteArray pngBytes = iconRead ? iconFile.readAll() : QByteArray();
        const QString replacementPath =
            QDir(ctx.workDirectory).filePath(QStringLiteral("chprops-replacement-probe.png"));
        QFile probeFile(replacementPath);
        const bool probeWritten = probeFile.open(QIODevice::WriteOnly) && probeFile.write(pngBytes) == pngBytes.size();
        probeFile.close();
        QString probeError;
        const bool probeLoaded =
            probeWritten && !pngBytes.isEmpty() && dialog.loadImageFromFile(replacementPath, &probeError);
        QFile::remove(replacementPath);
        ctx.check(!pngBytes.isEmpty(), QStringLiteral("图片替换探针可读取应用图标资源"),
                  QString::number(pngBytes.size()));
        ctx.check(probeWritten, QStringLiteral("图片替换探针可写入临时图片"), replacementPath);
        ctx.check(probeLoaded, QStringLiteral("图片属性窗可从文件载入替换图片"),
                  probeLoaded ? QStringLiteral("ok") : probeError);
        QPixmap replacement(12, 12);
        replacement.fill(Qt::blue);
        dialog.setReplacementImage(replacement);
        dialog.accept();
        QApplication::processEvents();
    }
    ctx.check(imageItem->image().size() == QSize(12, 12), QStringLiteral("图片本体替换生效"),
              QStringLiteral("%1x%2").arg(imageItem->image().width()).arg(imageItem->image().height()));
    ctx.check(imageItem->contentSize() == QSizeF(12.0, 12.0), QStringLiteral("图片本体替换后重置为原始像素尺寸"),
              QStringLiteral("%1x%2").arg(imageItem->contentSize().width()).arg(imageItem->contentSize().height()));
    ctx.check(editUndo.index() == imageBodyUndoIndex + 1, QStringLiteral("图片本体替换进入属性撤销记录"),
              QStringLiteral("%1 / %2").arg(imageBodyUndoIndex).arg(editUndo.index()));
    editUndo.undo();
    QApplication::processEvents();
    ctx.check(imageItem->image().size() == QSize(24, 24), QStringLiteral("图片本体替换可撤销"));
    ctx.check(imageItem->contentSize() == QSizeF(80.0, 80.0), QStringLiteral("图片本体撤销恢复旧尺寸"));

    // 需求 42：非编辑模式打开属性窗时字段只读，关闭不会写回。
    const int readOnlyUndoBefore = editUndo.count();
    const QString readOnlyImageName = imageItem->imageName();
    {
        ItemPropertiesDialog dialog(&editScene, imageItem, nullptr, false);
        dialog.show();
        QApplication::processEvents();
        QLineEdit* readOnlyName = dialog.findChild<QLineEdit*>(QStringLiteral("ItemPropertiesImageValue"));
        QPushButton* readOnlyReplace =
            dialog.findChild<QPushButton*>(QStringLiteral("ItemPropertiesReplaceImageValue"));
        QLabel* readOnlyHint = dialog.findChild<QLabel*>(QStringLiteral("ItemPropertiesReadOnlyHint"));
        ctx.check(dialog.isModal() && !dialog.isEditable(), QStringLiteral("属性窗在只读口径下仍为模态"));
        ctx.check(readOnlyName && !readOnlyName->isEnabled() && readOnlyReplace && !readOnlyReplace->isEnabled(),
                  QStringLiteral("非编辑模式属性窗字段与替换按钮不可编辑"));
        ctx.check(readOnlyHint != nullptr, QStringLiteral("非编辑模式属性窗显示只读提示"));
        if (readOnlyName) {
            readOnlyName->setText(QStringLiteral("read-only-probe.png"));
        }
        dialog.accept();
        QApplication::processEvents();
    }
    ctx.check(imageItem->imageName() == readOnlyImageName && editUndo.count() == readOnlyUndoBefore,
              QStringLiteral("只读属性窗关闭后不写回图元"));
}
