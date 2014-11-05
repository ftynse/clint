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
  painter->fillPath(m_arrowHead, QBrush(Qt::black));
}

QRectF VizDepArrow::boundingRect() const {
  // Since arrow head has a 60-degree angle, it will be covered by a rectangle
  // (with a 90-degree angle, obviously) ending at the head's sharp end.  This sharp end
  // lies on the arrow line, pointRadius pixels after the arrow line ends.
  const double pointRadius =
      m_polyhedron->coordinateSystem()->projection()->vizProperties()->pointRadius();
  QLineF diagonal(m_arrowLine);
  diagonal.setLength(diagonal.length() + pointRadius);
  return QRectF(m_arrowLine.p1(), m_arrowLine.p2());
}

void VizDepArrow::pointLink(QPointF source, QPointF target) {
  // Set the line so that it starts on the border of the first point's circle, and ends pointRadius
  // pixels before the second point's circle in order to put the arrow head there.
  const double pointRadius =
      m_polyhedron->coordinateSystem()->projection()->vizProperties()->pointRadius();
  m_arrowLine = QLineF(source, target);
  qreal length = m_arrowLine.length();
  QLineF displacementLine = QLineF(m_arrowLine);
  displacementLine.setLength(pointRadius);
  // Now p2 of displacement line has the coordinates p1 of the shorter line should have.
  m_arrowLine.setLength(length - 2. * pointRadius);
  m_arrowLine.setP1(displacementLine.p2());

  // Compute an arrow path of the style )>.
  // Head is based on the triangle with equal sides and an arc.
  // Center of the arc touches the arrow line, side angles are 1/4 R back along this line.
  // Hence triangle height is H = R + 1/4 R.
  // Thus length of the triangle side is, L = 5 / 2 / sqrt(3) R,
  // where R is point readius.
  const double halfside = 5.0 * pointRadius / 4.0 / sqrt(3.0);
  QPainterPath headPath;
  headPath.moveTo(-halfside, pointRadius / 4.0);
  headPath.lineTo(0, -pointRadius);
  headPath.lineTo(halfside, pointRadius / 4.0);
  const double angle = acos(halfside / pointRadius);
  headPath.arcTo(-pointRadius, 0, 2.0 * pointRadius, 2.0 * pointRadius,
                 90 - (angle * 180) / M_PI,
                 2 * (angle * 180) / M_PI);
  QTransform transform;
  transform.rotate(m_arrowLine.angle() + 180);
  headPath = transform.map(headPath);
  transform.reset();
  transform.translate(m_arrowLine.p2().x(), m_arrowLine.p2().y());
  m_arrowHead = transform.map(headPath);
}
