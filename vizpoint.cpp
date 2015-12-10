#include <QtGui>
#include <QtWidgets>
#include "vizpoint.h"
#include "vizprojection.h"

const int VizPoint::NO_COORD;

VizPoint::VizPoint(VizPolyhedron *polyhedron) :
  QGraphicsObject(polyhedron), m_polyhedron(polyhedron) {

  setFlag(QGraphicsItem::ItemIsSelectable);
  setFlag(QGraphicsItem::ItemIsMovable);
  setFlag(QGraphicsItem::ItemSendsGeometryChanges);

  if (m_polyhedron) {
    VizProperties *props = m_polyhedron->coordinateSystem()->projection()->vizProperties();
    if (props->filledPoints() && m_polyhedron->occurrence()) {
      setColor(props->color(m_polyhedron->occurrence()->betaVector()));
    } else {
      setColor(QColor::fromRgb(100, 100, 100, 127));
    }
  }
}

void VizPoint::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) {
  Q_UNUSED(widget);
  painter->save();
  const double radius =
      m_polyhedron->coordinateSystem()->projection()->vizProperties()->pointRadius();
  if (option->state & QStyle::State_Selected)
    painter->setBrush(QBrush(Qt::white));
  else
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
  } else if (change == QGraphicsItem::ItemPositionHasChanged) {
    emit positionChanged();
  }
  return QGraphicsItem::itemChange(change, value);
}

void VizPoint::mousePressEvent(QGraphicsSceneMouseEvent *event) {
  if (coordinateSystem()->projection()->selectionManager()->selectedPoints().empty()) {
    polyhedron()->mousePressEvent(event);
    return;
  }
  polyhedron()->hideHandles();

  if (event->button() == Qt::RightButton) {
    coordinateSystem()->projection()->manipulationManager()->pointRightClicked(this);
    return;
  }

  QGraphicsItem::mousePressEvent(event);
  m_pressPos = pos();
  coordinateSystem()->projection()->manipulationManager()->pointAboutToMove(this);
}

void VizPoint::mouseReleaseEvent(QGraphicsSceneMouseEvent *event) {
  if (coordinateSystem()->projection()->selectionManager()->selectedPoints().empty()) {
    polyhedron()->mouseReleaseEvent(event);
    return;
  }
  QGraphicsItem::mouseReleaseEvent(event);
  m_pressPos = QPointF(0,0);
  coordinateSystem()->projection()->manipulationManager()->pointHasMoved(this);
}

void VizPoint::mouseMoveEvent(QGraphicsSceneMouseEvent *event) {
  if (coordinateSystem()->projection()->selectionManager()->selectedPoints().empty()) {
    polyhedron()->mouseMoveEvent(event);
    return;
  }
  QGraphicsItem::mouseMoveEvent(event);
  QPointF diff = pos() - m_pressPos;
  coordinateSystem()->projection()->manipulationManager()->pointMoving(diff);
}

void VizPoint::setOriginalCoordinates(int horizontal, int vertical) {
  m_originalHorizontal = horizontal;
  m_originalVertical = vertical;
}

void VizPoint::setScatteredCoordinates(int horizontal, int vertical) {
  m_scatteredHorizontal = horizontal;
  m_scatteredVertical = vertical;
}

ClintStmt *VizPoint::statement() const {
  return m_polyhedron->statement();
}

ClintScop *VizPoint::scop() const {
  return m_polyhedron->scop();
}

ClintProgram *VizPoint::program() const {
  return m_polyhedron->program();
}

VizCoordinateSystem *VizPoint::coordinateSystem() const {
  return m_polyhedron->coordinateSystem();
}
