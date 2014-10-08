#include "macros.h"
#include "vizpolyhedron.h"
#include "vizpoint.h"

#include <QtGui>

VizPolyhedron::VizPolyhedron(QGraphicsItem *parent) :
  QGraphicsObject(parent) {
}

void VizPolyhedron::setPointVisiblePos(VizPoint *vp, int x, int y) {
  vp->setPos(x * VIZ_POINT_DISTANCE, y * VIZ_POINT_DISTANCE);
}

void VizPolyhedron::setProjectedPoints(std::vector<std::vector<int>> &&points,
                                       int horizontalMin,
                                       int verticalMin) {
  m_localHorizontalMin = horizontalMin;
  m_localVerticalMin   = verticalMin;
  for (const std::vector<int> &point : points) {
    VizPoint *vp = new VizPoint(this);
    if (point.size() == 0) {
      vp->setScatteredCoordinates();
      vp->setOriginalCoordinates();
      setPointVisiblePos(vp, 0, 0);
    } else if (point.size() == 2) {
      vp->setOriginalCoordinates(point[1]);
      vp->setScatteredCoordinates(point[0]);
      setPointVisiblePos(vp, point[0] - horizontalMin + 1, 0);
    } else if (point.size() == 4) {
      vp->setOriginalCoordinates(point[2], point[3]);
      vp->setScatteredCoordinates(point[0], point[1]);
      setPointVisiblePos(vp, point[0] - horizontalMin + 1, point[1] - verticalMin + 1);
    } else {
      CLINT_ASSERT(!"unreachable", "Point has wrong number of dimensions");
    }
    m_points.insert(vp);
    // TODO: setup alpha-beta vector for point
    // alphas that are not in the current projection are wildcards for this point, can't get them
    // Or it can be reconstructed using respective parent polyhedron and coodrinate system
  }
}

void VizPolyhedron::adjustProjectedPoints(int coordinateSystemOriginX, int coordinateSystemOriginY) {
  int horizontalAdjustment = -coordinateSystemOriginX + 1;
  int verticalAdjustment = -coordinateSystemOriginY + 1;
  for (VizPoint *vp : m_points) {
    int x, y;
    std::tie(x, y) = vp->scatteredCoordinates();
    if (x == VizPoint::NO_COORD) x = 0;
    if (y == VizPoint::NO_COORD) y = 0;
    setPointVisiblePos(vp, x + horizontalAdjustment, y + verticalAdjustment);
  }
}

void VizPolyhedron::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) {
  Q_UNUSED(option);
  Q_UNUSED(widget);
  painter->save();
  // FIXME: hardcoded values
  QRectF rect = childrenBoundingRect().adjusted(-8, -8, 8, 8);
  painter->drawRect(rect);
  painter->restore();
}

QRectF VizPolyhedron::boundingRect() const {
  return childrenBoundingRect().adjusted(-10, -10, 10, 10);
}

QPainterPath VizPolyhedron::shape() const {
  return QGraphicsItem::shape();
}
