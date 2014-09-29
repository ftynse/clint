#include "macros.h"
#include "vizpolyhedron.h"
#include "vizpoint.h"

#include <QtGui>

VizPolyhedron::VizPolyhedron(QGraphicsItem *parent) :
  QGraphicsObject(parent) {
}

void VizPolyhedron::setupFromPoints__(std::vector<std::vector<int> > &&points) {
  for (const std::vector<int> &point : points) {
    VizPoint *vp = new VizPoint(this);
    if (point.size() == 0) {
      vp->setScatteredCoordinates();
      vp->setOriginalCoordinates();
    } else if (point.size() == 2) {
      vp->setOriginalCoordinates(point[1]);
      vp->setScatteredCoordinates(point[0]);
    } else if (point.size() == 4) {
      vp->setOriginalCoordinates(point[2], point[3]);
      vp->setScatteredCoordinates(point[0], point[1]);
    } else {
      CLINT_ASSERT(!"unreachable", "Point has wrong number of dimensions");
    }
    // TODO: setup alpha-beta vector for point
    // This will require to pass the beta-vector all along from the ClintProjection
    // Or it can be reconstructed using respective parent polyhedron and coodrinate system
  }
}

void VizPolyhedron::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) {
  Q_UNUSED(option);
  Q_UNUSED(widget);
  painter->save();
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
