#include "ui/icons/IconFileEditIcons.h"

#include <QPainter>
#include <QPainterPath>

namespace {

void DrawNew(QPainter& painter, IconId)
{
    QPainterPath page;
    page.moveTo(5.0, 3.0);
    page.lineTo(14.0, 3.0);
    page.lineTo(18.0, 7.0);
    page.lineTo(18.0, 12.5);
    painter.drawPath(page);
    painter.drawLine(QPointF(14.0, 3.0), QPointF(14.0, 7.0));
    painter.drawLine(QPointF(14.0, 7.0), QPointF(18.0, 7.0));
    painter.drawLine(QPointF(5.0, 3.0), QPointF(5.0, 21.0));
    painter.drawLine(QPointF(5.0, 21.0), QPointF(13.0, 21.0));
    painter.setBrush(painter.pen().color());
    painter.drawEllipse(QPointF(18.0, 18.0), 4.5, 4.5);
    painter.setPen(QPen(Qt::white, 1.6, Qt::SolidLine, Qt::RoundCap));
    painter.drawLine(QPointF(18.0, 15.8), QPointF(18.0, 20.2));
    painter.drawLine(QPointF(15.8, 18.0), QPointF(20.2, 18.0));
}

void DrawOpen(QPainter& painter, IconId)
{
    QPainterPath folder;
    folder.moveTo(3.0, 18.5);
    folder.lineTo(3.0, 6.0);
    folder.lineTo(9.0, 6.0);
    folder.lineTo(11.0, 8.5);
    folder.lineTo(19.0, 8.5);
    folder.lineTo(19.0, 10.5);
    painter.drawPath(folder);
    QPainterPath front;
    front.moveTo(3.0, 18.5);
    front.lineTo(6.0, 11.0);
    front.lineTo(22.0, 11.0);
    front.lineTo(19.0, 18.5);
    front.closeSubpath();
    painter.drawPath(front);
}

void DrawSave(QPainter& painter, IconId)
{
    painter.drawRoundedRect(QRectF(3.0, 3.0, 18.0, 18.0), 2.0, 2.0);
    painter.drawRect(QRectF(7.0, 3.0, 9.0, 6.0));
    painter.drawRoundedRect(QRectF(6.0, 13.0, 12.0, 7.0), 1.2, 1.2);
    painter.drawLine(QPointF(9.0, 3.0), QPointF(9.0, 6.5));
}

void DrawExport(QPainter& painter, IconId)
{
    painter.drawLine(QPointF(12.0, 3.0), QPointF(12.0, 14.0));
    painter.drawLine(QPointF(12.0, 3.0), QPointF(8.0, 7.0));
    painter.drawLine(QPointF(12.0, 3.0), QPointF(16.0, 7.0));
    QPainterPath tray;
    tray.moveTo(4.0, 13.0);
    tray.lineTo(4.0, 20.0);
    tray.lineTo(20.0, 20.0);
    tray.lineTo(20.0, 13.0);
    painter.drawPath(tray);
}

void DrawUndo(QPainter& painter, IconId)
{
    QPainterPath arc;
    arc.moveTo(19.0, 18.0);
    arc.cubicTo(20.0, 10.0, 13.0, 4.5, 5.5, 8.0);
    painter.drawPath(arc);
    painter.drawLine(QPointF(5.5, 8.0), QPointF(10.5, 4.5));
    painter.drawLine(QPointF(5.5, 8.0), QPointF(10.5, 11.5));
}

void DrawRedo(QPainter& painter, IconId)
{
    painter.save();
    painter.translate(24.0, 0.0);
    painter.scale(-1.0, 1.0);
    QPainterPath arc;
    arc.moveTo(19.0, 18.0);
    arc.cubicTo(20.0, 10.0, 13.0, 4.5, 5.5, 8.0);
    painter.drawPath(arc);
    painter.drawLine(QPointF(5.5, 8.0), QPointF(10.5, 4.5));
    painter.drawLine(QPointF(5.5, 8.0), QPointF(10.5, 11.5));
    painter.restore();
}

void DrawBrush(QPainter& painter, IconId)
{
    QPainterPath handle;
    handle.moveTo(10.0, 15.5);
    handle.lineTo(18.5, 6.0);
    handle.cubicTo(19.8, 4.6, 21.5, 6.5, 20.0, 7.8);
    handle.lineTo(11.8, 17.5);
    handle.closeSubpath();
    painter.drawPath(handle);
    QPainterPath tip;
    tip.moveTo(10.4, 16.6);
    tip.lineTo(8.0, 21.0);
    tip.lineTo(4.5, 17.8);
    tip.lineTo(9.0, 15.0);
    tip.closeSubpath();
    painter.drawPath(tip);
}

void DrawEdit(QPainter& painter, IconId)
{
    QPainterPath pointer;
    pointer.moveTo(5.5, 3.0);
    pointer.lineTo(5.5, 19.5);
    pointer.lineTo(9.8, 15.2);
    pointer.lineTo(13.0, 21.5);
    pointer.lineTo(16.0, 20.0);
    pointer.lineTo(12.8, 13.8);
    pointer.lineTo(18.5, 13.4);
    pointer.closeSubpath();
    painter.setBrush(Qt::white);
    painter.drawPath(pointer);
}

void DrawText(QPainter& painter, IconId)
{
    painter.drawLine(QPointF(4.5, 5.0), QPointF(19.5, 5.0));
    painter.drawLine(QPointF(12.0, 5.0), QPointF(12.0, 20.0));
    painter.drawLine(QPointF(8.0, 20.0), QPointF(16.0, 20.0));
    painter.drawLine(QPointF(5.5, 8.0), QPointF(5.5, 5.0));
    painter.drawLine(QPointF(18.5, 8.0), QPointF(18.5, 5.0));
}

void DrawDelete(QPainter& painter, IconId)
{
    painter.drawLine(QPointF(4.0, 6.5), QPointF(20.0, 6.5));
    painter.drawLine(QPointF(9.5, 6.5), QPointF(9.5, 3.5));
    painter.drawLine(QPointF(9.5, 3.5), QPointF(14.5, 3.5));
    painter.drawLine(QPointF(14.5, 3.5), QPointF(14.5, 6.5));
    QPainterPath bin;
    bin.moveTo(6.0, 6.5);
    bin.lineTo(7.5, 21.0);
    bin.lineTo(16.5, 21.0);
    bin.lineTo(18.0, 6.5);
    painter.drawPath(bin);
    painter.drawLine(QPointF(10.0, 10.0), QPointF(10.5, 17.5));
    painter.drawLine(QPointF(14.0, 10.0), QPointF(13.5, 17.5));
}

void DrawFolder(QPainter& painter, IconId)
{
    QPainterPath folder;
    folder.moveTo(3.0, 19.0);
    folder.lineTo(3.0, 5.0);
    folder.lineTo(9.5, 5.0);
    folder.lineTo(11.5, 7.5);
    folder.lineTo(21.0, 7.5);
    folder.lineTo(21.0, 19.0);
    folder.closeSubpath();
    painter.drawPath(folder);
    painter.drawLine(QPointF(3.0, 10.0), QPointF(21.0, 10.0));
}

void DrawSettings(QPainter& painter, IconId)
{
    painter.drawLine(QPointF(3.0, 6.5), QPointF(21.0, 6.5));
    painter.drawLine(QPointF(3.0, 12.0), QPointF(21.0, 12.0));
    painter.drawLine(QPointF(3.0, 17.5), QPointF(21.0, 17.5));
    painter.setBrush(Qt::white);
    painter.drawEllipse(QPointF(16.0, 6.5), 2.6, 2.6);
    painter.drawEllipse(QPointF(8.0, 12.0), 2.6, 2.6);
    painter.drawEllipse(QPointF(15.0, 17.5), 2.6, 2.6);
}

constexpr struct IconEntry {
    IconId id;
    void (*draw)(QPainter&, IconId);
} ENTRIES[] = {
    {IconId::NEW, DrawNew},       {IconId::OPEN, DrawOpen},     {IconId::SAVE, DrawSave},
    {IconId::EXPORT, DrawExport}, {IconId::UNDO, DrawUndo},     {IconId::REDO, DrawRedo},
    {IconId::BRUSH, DrawBrush},   {IconId::EDIT, DrawEdit},     {IconId::TEXT, DrawText},
    {IconId::DELETE, DrawDelete}, {IconId::FOLDER, DrawFolder}, {IconId::SETTINGS, DrawSettings},
};

} // namespace

bool PaintIconFileEdit(QPainter& painter, IconId id)
{
    for (const IconEntry& entry : ENTRIES) {
        if (entry.id == id) {
            entry.draw(painter, id);
            return true;
        }
    }
    return false;
}
