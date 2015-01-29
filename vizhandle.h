#ifndef VIZHANDLE_H
#define VIZHANDLE_H

#include <QObject>
#include <QGraphicsRectItem>
#include <QList>

class VizHandle : public QObject, public QGraphicsRectItem {
  Q_OBJECT
public:
  enum class Kind {
    LEFT,
    RIGHT,
    TOP,
    BOTTOM,
    TOPLEFT,
    TOPRIGHT,
    BOTTOMLEFT,
    BOTTOMRIGHT
  };

  VizHandle(QGraphicsItem *item, Kind kind);

  Kind kind() const {
    return m_kind;
  }

  static QList<VizHandle *> createCornerHandles(QGraphicsItem *item) {
    QList<VizHandle *> list;
    list.append(new VizHandle(item, Kind::TOPLEFT));
    list.append(new VizHandle(item, Kind::TOPRIGHT));
    list.append(new VizHandle(item, Kind::BOTTOMLEFT));
    list.append(new VizHandle(item, Kind::BOTTOMRIGHT));
    return list;
  }

  static QList<VizHandle *> createCenterHandles(QGraphicsItem *item) {
    QList<VizHandle *> list;
    list.append(new VizHandle(item, Kind::LEFT));
    list.append(new VizHandle(item, Kind::RIGHT));
    list.append(new VizHandle(item, Kind::TOP));
    list.append(new VizHandle(item, Kind::BOTTOM));
    return list;
  }

  static QList<VizHandle *> createCenterCornerHandles(QGraphicsItem *item) {
    QList<VizHandle *> list = createCenterHandles(item);
    list.append(createCornerHandles(item));
    return list;
  }

  void recomputePos();

  void mousePressEvent(QGraphicsSceneMouseEvent *event);
  void mouseMoveEvent(QGraphicsSceneMouseEvent *event);
  void mouseReleaseEvent(QGraphicsSceneMouseEvent *event);

  void hoverEnterEvent(QGraphicsSceneHoverEvent *event);
  void hoverLeaveEvent(QGraphicsSceneHoverEvent *event);

protected:
  QVariant itemChange(GraphicsItemChange change, const QVariant &value);

signals:
  void aboutToMove(const VizHandle *const handle);
  void moving(const VizHandle *const handle, QPointF displacement);
  void hasMoved(const VizHandle *const handle, QPointF displacement);

private:

  Kind m_kind;
  double m_size = 6; // FIXME: hardcode, take from vizproperties

  QPointF m_pressPos = QPointF();
  bool    m_pressed  = false;
};

#endif // VIZHANDLE_H
