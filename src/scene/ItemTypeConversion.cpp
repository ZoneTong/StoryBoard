#include "scene/ItemTypeConversion.h"

#include "app/I18n.h"
#include "items/CanvasItem.h"
#include "items/SectorBase.h"
#include "items/lines/AngleItem.h"
#include "items/lines/ArcItem.h"
#include "items/lines/LineAbstractItem.h"
#include "items/shapes/SectorItem.h"
#include "items/shapes/ShapeAbstractItem.h"

namespace {

const QString KIND_SECTOR = QStringLiteral("sector");
const QString KIND_ARC = QStringLiteral("arc");
const QString KIND_ANGLE = QStringLiteral("angle");

struct CommonGeometry {
    QPointF pos;
    QSizeF size;
    qreal rotation = 0.0;
    QRectF frameLocal;
    qreal startAngle = 30.0;
    qreal sweepAngle = 120.0;
    bool flipX = false;
    bool flipY = false;
    QColor stroke;
    qreal strokeWidth = 1.0;
    bool hasFill = false;
    QColor fillColor;
    FillStyle fillStyle = FillStyle::NONE;
};

bool ReadCommon(const CanvasItem* item, CommonGeometry* common)
{
    const auto* ellipse = dynamic_cast<const SectorBase*>(item);
    if (!ellipse) {
        return false;
    }
    common->pos = item->pos();
    common->size = item->ContentSize();
    common->rotation = item->RotationAngle();
    common->frameLocal = ellipse->EllipseFrameLocal(common->size);
    common->startAngle = ellipse->StartAngle();
    common->sweepAngle = ellipse->SweepAngle();
    common->flipX = item->FlipX();
    common->flipY = item->FlipY();
    if (const auto* line = dynamic_cast<const LineAbstractItem*>(item)) {
        common->stroke = line->StrokeColor();
        common->strokeWidth = line->StrokeWidth();
        return true;
    }
    if (const auto* shape = dynamic_cast<const ShapeAbstractItem*>(item)) {
        common->stroke = shape->StrokeColor();
        common->strokeWidth = shape->StrokeWidth();
        common->hasFill = true;
        common->fillColor = shape->FillColor();
        common->fillStyle = shape->fillStyle();
    }
    return true;
}

// 线型图元的原点由首个笔迹点决定，造完要按“椭圆框中心”的差把整体挪回原图元的位置。
void AlignFrameCenter(CanvasItem* converted, const CanvasItem* source, const QRectF& frameLocal)
{
    const QPointF sourceCenter = source->mapToScene(frameLocal.center());
    const QPointF targetCenter = converted->mapToScene(converted->FrameRect().center());
    converted->setPos(converted->pos() + (sourceCenter - targetCenter));
}

QColor DefaultStroke()
{
    return QColor(QStringLiteral("#1E293B"));
}

} // namespace

namespace ItemTypeConversion {

QString TypeOf(const CanvasItem* item)
{
    if (!item) {
        return QString();
    }
    if (const auto* sector = dynamic_cast<const SectorItem*>(item)) {
        // 自定义轮廓已经不是“椭圆上的截段”，没有起止角语义，不参与互换。
        return sector->IsCustomShape() ? QString() : KIND_SECTOR;
    }
    if (dynamic_cast<const ArcItem*>(item)) {
        return KIND_ARC;
    }
    if (dynamic_cast<const AngleItem*>(item)) {
        return KIND_ANGLE;
    }
    return QString();
}

QStringList TargetsOf(const CanvasItem* item)
{
    const QString type = TypeOf(item);
    if (type.isEmpty()) {
        return {};
    }
    QStringList targets;
    for (const QString& kind : {KIND_ARC, KIND_ANGLE, KIND_SECTOR}) {
        if (kind != type) {
            targets.append(kind);
        }
    }
    return targets;
}

QString DisplayNameOf(const QString& kind)
{
    if (kind == KIND_ARC) {
        return UiText(QStringLiteral("Arc"));
    }
    if (kind == KIND_ANGLE) {
        return UiText(QStringLiteral("Angle line"));
    }
    if (kind == KIND_SECTOR) {
        return UiText(QStringLiteral("Sector"));
    }
    return QString();
}

CanvasItem* CreateConverted(const CanvasItem* source, const QString& kind)
{
    CommonGeometry common;
    if (!source || !ReadCommon(source, &common)) {
        return nullptr;
    }
    const bool fromLine = dynamic_cast<const LineAbstractItem*>(source) != nullptr;
    const QColor stroke = common.stroke.isValid() ? common.stroke : DefaultStroke();

    if (kind == KIND_ARC) {
        auto* arc = new ArcItem();
        arc->SetArcGeometry(common.frameLocal.topLeft(), common.frameLocal.size(), common.startAngle, common.sweepAngle,
                            stroke, common.strokeWidth);
        if (fromLine) {
            const auto* line = static_cast<const LineAbstractItem*>(source);
            arc->SetArrowStyles(line->StartArrow(), line->EndArrow());
        }
        AlignFrameCenter(arc, source, common.frameLocal);
        arc->SetRotationAngle(common.rotation);
        arc->SetFlips(common.flipX, common.flipY);
        return arc;
    }
    if (kind == KIND_ANGLE) {
        auto* angle = new AngleItem();
        angle->SetAngleGeometry(common.frameLocal.topLeft(), common.frameLocal.size(), common.startAngle,
                                common.sweepAngle, stroke, common.strokeWidth);
        if (fromLine) {
            const auto* line = static_cast<const LineAbstractItem*>(source);
            angle->SetArrowStyles(line->StartArrow(), line->EndArrow());
        }
        AlignFrameCenter(angle, source, common.frameLocal);
        angle->SetRotationAngle(common.rotation);
        angle->SetFlips(common.flipX, common.flipY);
        return angle;
    }
    if (kind == KIND_SECTOR) {
        // 进扇形有损：描线箭头没有意义，直接丢弃；图形的位置就是内容框左上角。
        auto* sector = new SectorItem();
        sector->setPos(common.pos);
        sector->SetContentSize(common.size);
        sector->SetArcAngles(common.startAngle, common.sweepAngle);
        sector->SetStrokeColor(stroke);
        sector->SetStrokeWidth(common.strokeWidth);
        if (common.hasFill) {
            sector->SetFill(common.fillColor, common.fillStyle);
        }
        sector->SetRotationAngle(common.rotation);
        sector->SetFlips(common.flipX, common.flipY);
        return sector;
    }
    return nullptr;
}

} // namespace ItemTypeConversion
