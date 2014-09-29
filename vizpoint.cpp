#include <QtGui>
#include <QtWidgets>
#include "vizpoint.h"

const int VizPoint::NO_COORD;

VizPoint::VizPoint(QGraphicsItem *parent) :
  QGraphicsObject(parent) {
}

void VizPoint::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) {
  Q_UNUSED(option);
  Q_UNUSED(widget);
  painter->save();
  painter->drawEllipse(QPoint(0, 0), VIZ_POINT_RADIUS, VIZ_POINT_RADIUS);
  painter->restore();
}

QRectF VizPoint::boundingRect() const {
  return QRectF(-VIZ_POINT_RADIUS, -VIZ_POINT_RADIUS, 2*VIZ_POINT_RADIUS, 2*VIZ_POINT_RADIUS);
}

QPainterPath VizPoint::shape() const {
  QPainterPath path;
  path.addEllipse(boundingRect());
  return path;
}

void VizPoint::setOriginalCoordinates(int horizontal, int vertical) {
  m_originalHorizontal = horizontal;
  m_originalVertical = vertical;
}

void VizPoint::setScatteredCoordinates(int horizontal, int vertical) {
  m_scatteredHorizontal = horizontal;
  m_scatteredVertical = vertical;
  qreal posX = horizontal == NO_COORD ? 0 : VIZ_POINT_DISTANCE * horizontal;
  qreal posY = vertical == NO_COORD ? 0 : VIZ_POINT_DISTANCE * vertical;
  setPos(posX, posY);
}
