#include <QtGui>
#include <QtWidgets>
#include "vizpoint.h"
#include "vizprojection.h"

const int VizPoint::NO_COORD;

VizPoint::VizPoint(VizPolyhedron *polyhedron) :
  QGraphicsObject(polyhedron), m_polyhedron(polyhedron) {

  setFlag(QGraphicsItem::ItemIsSelectable);
}

void VizPoint::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) {
  Q_UNUSED(option);
  Q_UNUSED(widget);
  painter->save();
  const double radius =
      m_polyhedron->coordinateSystem()->projection()->vizProperties()->pointRadius();
  painter->setBrush(QBrush(m_color));
  painter->drawEllipse(QPointF(0, 0), radius, radius);
  painter->restore();
}

QRectF VizPoint::boundingRect() const {
  const double radius =
      m_polyhedron->coordinateSystem()->projection()->vizProperties()->pointRadius();
  return QRectF(-radius, -radius, 2*radius, 2*radius);
}

QPainterPath VizPoint::shape() const {
  QPainterPath path;
  path.addEllipse(boundingRect());
  return path;
}

QVariant VizPoint::itemChange(GraphicsItemChange change, const QVariant &value) {
  if (change == QGraphicsItem::ItemSelectedHasChanged) {
    coordinateSystem()->projection()->selectionManager()->pointSelectionChanged(this, value.toBool());
  }
  return QGraphicsItem::itemChange(change, value);
}

void VizPoint::setOriginalCoordinates(int horizontal, int vertical) {
  m_originalHorizontal = horizontal;
  m_originalVertical = vertical;
}

void VizPoint::setScatteredCoordinates(int horizontal, int vertical) {
  m_scatteredHorizontal = horizontal;
  m_scatteredVertical = vertical;
}
