#include "vizdeparrow.h"
#include "vizpolyhedron.h"
#include "vizprojection.h"

#include <QtGui>
#include <QtWidgets>

VizDepArrow::VizDepArrow(QPointF source, QPointF target, QGraphicsItem *parent) :
  QGraphicsItem(parent) {

  m_polyhedron = qgraphicsitem_cast<VizPolyhedron *>(parent);
  CLINT_ASSERT(m_polyhedron != nullptr,
               "Dependence arrow should belong to a polyhedron");
  pointLink(source, target);
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

void VizDepArrow::pointLink(QPointF source, QPointF target) {
  const double pointRadius =
      m_polyhedron->coordinateSystem()->projection()->vizProperties()->pointRadius();
  m_arrowLine = QLineF(source, target);
  qreal length = m_arrowLine.length();
  QLineF displacementLine = QLineF(m_arrowLine);
  displacementLine.setLength(pointRadius);
  // Now p2 of displacement line has the coordinates p1 of the shorter line should have.
  m_arrowLine.setLength(length - 2. * pointRadius);
  m_arrowLine.setP1(displacementLine.p2());

  // TODO: draw a proper arrow here
  m_arrowHead.addEllipse(m_arrowLine.p2(), 0.3 * pointRadius, 0.3 * pointRadius);
}
