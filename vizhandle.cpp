#include "vizhandle.h"
#include "macros.h"
#include <QtCore>
#include <QtGui>

#include <QtDebug>

constexpr int VizHandle::updates[VizHandle::NB_KINDS][VizHandle::NB_KINDS];

VizHandle::VizHandle(QGraphicsItem *item, Kind kind) :
  QGraphicsRectItem(item), m_kind(kind) {

  setFlag(QGraphicsItem::ItemSendsGeometryChanges);
  setFlag(QGraphicsItem::ItemIsMovable);
  setAcceptHoverEvents(true);

  recomputePos();
  setZValue(1000);

  setBrush(QBrush(Qt::black));
}

void VizHandle::recomputePos() {
  double x,y;
  if (!parentItem())
    return;

  setPos(0, 0);

  QRectF parentBoundingRect = parentItem()->boundingRect();

  switch (m_kind) {
  case Kind::LEFT:
    x = parentBoundingRect.left();
    y = parentBoundingRect.center().y();
    break;
  case Kind::RIGHT:
    x = parentBoundingRect.right();
    y = parentBoundingRect.center().y();
    break;
  case Kind::TOP:
    x = parentBoundingRect.center().x();
    y = parentBoundingRect.top();
    break;
  case Kind::BOTTOM:
    x = parentBoundingRect.center().x();
    y = parentBoundingRect.bottom();
    break;
  case Kind::TOPLEFT:
    x = parentBoundingRect.left();
    y = parentBoundingRect.top();
    break;
  case Kind::TOPRIGHT:
    x = parentBoundingRect.right();
    y = parentBoundingRect.top();
    break;
  case Kind::BOTTOMLEFT:
    x = parentBoundingRect.left();
    y = parentBoundingRect.bottom();
    break;
  case Kind::BOTTOMRIGHT:
    x = parentBoundingRect.right();
    y = parentBoundingRect.bottom();
    break;
  default:
    CLINT_UNREACHABLE;
  }

  setRect(x - m_size / 2, y - m_size / 2, m_size, m_size);
}

QVariant VizHandle::itemChange(GraphicsItemChange change, const QVariant &value) {
  if (change == QGraphicsItem::ItemPositionChange && m_pressed) {
    QPointF oldPos = pos();
    QPointF newPos = value.toPointF();
    switch (m_kind) {
    case Kind::LEFT:
    case Kind::RIGHT:
      newPos.ry() = oldPos.y();
      break;

    case Kind::TOP:
    case Kind::BOTTOM:
      newPos.rx() = oldPos.x();
      break;
    default:
      break;
    }
    return QVariant::fromValue(newPos);
  }
  return QGraphicsRectItem::itemChange(change, value);
}

void VizHandle::mousePressEvent(QGraphicsSceneMouseEvent *event) {
  QGraphicsRectItem::mousePressEvent(event);

  emit aboutToMove(this);

  m_pressPos = pos();
  m_pressed = true;
}

void VizHandle::mouseReleaseEvent(QGraphicsSceneMouseEvent *event) {
  QGraphicsRectItem::mouseReleaseEvent(event);

  QPointF displacement = pos() - m_pressPos;
  emit hasMoved(this, displacement);
}

void VizHandle::mouseMoveEvent(QGraphicsSceneMouseEvent *event) {
  QGraphicsRectItem::mouseMoveEvent(event);
  if (m_pressed) {
    QPointF displacement = pos() - m_pressPos;
    if (!displacement.isNull()) {
      emit moving(this, displacement);
    }
  }
}

void VizHandle::hoverEnterEvent(QGraphicsSceneHoverEvent *event) {
  QGraphicsRectItem::hoverEnterEvent(event);
  setBrush(Qt::white);
}

void VizHandle::hoverLeaveEvent(QGraphicsSceneHoverEvent *event) {
  QGraphicsRectItem::hoverLeaveEvent(event);
  setBrush(Qt::black);
}

void VizHandle::startFollowing(const VizHandle * const handle) {
  Q_UNUSED(handle);
  m_originalPos = pos();
}

void VizHandle::connectHandles(QList<VizHandle *> &list) {
  for (VizHandle *v1 : list) {
    for (VizHandle *v2 : list) {
      if (v1 == v2 || VizHandle::updates[v1->m_kind][v2->m_kind] == NO_UPDATE)
        continue;
      connect(v1, &VizHandle::aboutToMove, v2, &VizHandle::startFollowing);
      connect(v1, &VizHandle::moving, v2, &VizHandle::follow);
    }
  }
}

void VizHandle::follow(const VizHandle * const handle, QPointF displacement) {
  int updateFlags = VizHandle::updates[handle->kind()][kind()];
  QPointF newPos = m_originalPos;

  if (updateFlags & HORIZONTAL_FULL) {
    newPos.rx() += displacement.x();
  } else if (updateFlags & HORIZONTAL_HALF) {
    newPos.rx() += displacement.x() / 2;
  }

  if (updateFlags & VERTICAL_FULL) {
    newPos.ry() += displacement.y();
  } else if (updateFlags & VERTICAL_HALF) {
    newPos.ry() += displacement.y() / 2;
  }
  setPos(newPos);
}
