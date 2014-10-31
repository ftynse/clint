#include "vizdeparrow.h"
#include "vizcoordinatesystem.h" // FIXME: needed for macros, when moved to config, remove

#include <QtGui>
#include <QtWidgets>

VizDepArrow::VizDepArrow(QGraphicsItem *parent) :
  QGraphicsItem(parent) {
}

void VizDepArrow::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) {
  Q_UNUSED(option);
  Q_UNUSED(widget);

  painter->drawLine(m_arrowLine);
  painter->drawPath(m_arrowHead);
}

QRectF VizDepArrow::boundingRect() const {
  // FIXME: calculate properly and only once
  return QRectF(m_arrowLine.p1(), m_arrowLine.p2()) |
         m_arrowHead.boundingRect();
}

VizDepArrow *VizDepArrow::pointLink(QPointF source, QPointF target) {
  VizDepArrow *arrow = new VizDepArrow;
  arrow->m_arrowLine = QLineF(source, target);
  qreal length = arrow->m_arrowLine.length();
  QLineF displacementLine = QLineF(arrow->m_arrowLine);
  displacementLine.setLength(VIZ_POINT_RADIUS);
  // Now p2 of displacement line has the coordinates p1 of the shorter line should have.
  arrow->m_arrowLine.setLength(length - 2. * VIZ_POINT_RADIUS);
  arrow->m_arrowLine.setP1(displacementLine.p2());

  // TODO: draw a proper arrow here
  arrow->m_arrowHead.addEllipse(arrow->m_arrowLine.p2(), 0.3 * VIZ_POINT_RADIUS, 0.3 * VIZ_POINT_RADIUS);
  return arrow;
}
