#include "items/polygon/PolygonAccess.h"

#include "items/CanvasItem.h"
#include "items/CanvasItemVisitor.h"
#include "items/polygon/PolygonItem.h"

namespace {

class PolygonFinder final : public CanvasItemVisitor {
  public:
    void VisitPolygon(PolygonItem& polygon) override { m_polygon = &polygon; }
    PolygonItem* Polygon() const { return m_polygon; }

  private:
    PolygonItem* m_polygon = nullptr;
};

} // namespace

namespace PolygonAccess {

PolygonItem* As(CanvasItem* item)
{
    if (!item) {
        return nullptr;
    }
    PolygonFinder finder;
    item->Accept(finder);
    return finder.Polygon();
}

bool Is(CanvasItem* item)
{
    return PolygonAccess::As(item) != nullptr;
}

} // namespace PolygonAccess
