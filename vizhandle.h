#ifndef VIZHANDLE_H
#define VIZHANDLE_H

#include <QObject>
#include <QGraphicsRectItem>
#include <QList>

class VizHandle : public QObject, public QGraphicsRectItem {
  Q_OBJECT
public:
  enum Kind {
    LEFT = 0,
    RIGHT,
    TOP,
    BOTTOM,
    TOPLEFT,
    TOPRIGHT,
    BOTTOMLEFT,
    BOTTOMRIGHT,
    NB_KINDS
  };

  VizHandle(QGraphicsItem *item, Kind kind);

  Kind kind() const {
    return m_kind;
  }

  static QList<VizHandle *> createCornerHandles(QGraphicsItem *item) {
    QList<VizHandle *> list;
    createCornerHandlesInternal(list, item);
    connectHandles(list);
    return list;
  }

  static QList<VizHandle *> createCenterHandles(QGraphicsItem *item) {
    QList<VizHandle *> list;
    createCenterHandlesInternal(list, item);
    connectHandles(list);
    return list;
  }

  static QList<VizHandle *> createCenterCornerHandles(QGraphicsItem *item) {
    QList<VizHandle *> list;
    createCenterHandlesInternal(list, item);
    createCornerHandlesInternal(list, item);
    connectHandles(list);
    return list;
  }

  bool isModifierPressed() const {
    return m_modifier;
  }

  void recomputePos();
  QPointF center() const {
    return rect().center();
  }

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

private slots:
  void startFollowing(const VizHandle *const handle);
  void follow(const VizHandle *const handle, QPointF displacement);

private:
  Kind m_kind;
  double m_size = 6; // FIXME: hardcode, take from vizproperties

  QPointF m_pressPos = QPointF();
  bool    m_pressed  = false;
  bool    m_modifier = false;

  QPointF m_originalPos;

  enum UpdateFlags {
    NO_UPDATE       = 0x0,
    HORIZONTAL_HALF = 0x1,
    HORIZONTAL_FULL = 0x2,
    VERTICAL_HALF   = 0x4,
    VERTICAL_FULL   = 0x8
  };

#define HOR_F_VER_H (HORIZONTAL_FULL | VERTICAL_HALF)
#define HOR_H_VER_F (HORIZONTAL_HALF | VERTICAL_FULL)
  // LEFT             RIGHT            TOP              BOTTOM           TOPLEFT          TOPRIGHT         BOTTOMLEFT       BOTTOMRIGHT   [v from][< to]
  constexpr static int updates[NB_KINDS][NB_KINDS] = {
    {NO_UPDATE,       NO_UPDATE,       HORIZONTAL_HALF, HORIZONTAL_HALF, HORIZONTAL_FULL, NO_UPDATE,       HORIZONTAL_FULL, NO_UPDATE},       // LEFT
    {NO_UPDATE,       NO_UPDATE,       HORIZONTAL_HALF, HORIZONTAL_HALF, NO_UPDATE,       HORIZONTAL_FULL, NO_UPDATE,       HORIZONTAL_FULL}, // RIGHT
    {VERTICAL_HALF,   VERTICAL_HALF,   NO_UPDATE,       NO_UPDATE,       VERTICAL_FULL,   VERTICAL_FULL,   NO_UPDATE,       NO_UPDATE},       // TOP
    {VERTICAL_HALF,   VERTICAL_HALF,   NO_UPDATE,       NO_UPDATE,       NO_UPDATE,       NO_UPDATE,       VERTICAL_FULL,   VERTICAL_FULL},   // BOTTOM
    {HOR_F_VER_H,     VERTICAL_HALF,   HOR_H_VER_F,     HORIZONTAL_HALF, NO_UPDATE,       VERTICAL_FULL,   HORIZONTAL_FULL, NO_UPDATE},       // TOPLEFT
    {VERTICAL_HALF,   HOR_F_VER_H,     HOR_H_VER_F,     HORIZONTAL_HALF, VERTICAL_FULL,   NO_UPDATE,       NO_UPDATE,       HORIZONTAL_FULL}, // TOPRIGHT
    {HOR_F_VER_H,     VERTICAL_HALF,   HORIZONTAL_HALF, HOR_H_VER_F,     HORIZONTAL_FULL, NO_UPDATE,       NO_UPDATE,       VERTICAL_FULL},   // BOTTOMLEFT
    {VERTICAL_HALF,   HOR_F_VER_H,     HORIZONTAL_HALF, HOR_H_VER_F,     NO_UPDATE,       HORIZONTAL_FULL, VERTICAL_FULL,   NO_UPDATE}        // BOTTOMRIGHT
  };
#undef HOR_F_VER_H
#undef HOR_H_VER_F

  static void connectHandles(QList<VizHandle *> &list);

  static void createCornerHandlesInternal(QList<VizHandle *> &list, QGraphicsItem *item) {
    list.append(new VizHandle(item, Kind::TOPLEFT));
    list.append(new VizHandle(item, Kind::TOPRIGHT));
    list.append(new VizHandle(item, Kind::BOTTOMLEFT));
    list.append(new VizHandle(item, Kind::BOTTOMRIGHT));
  }

  static void createCenterHandlesInternal(QList<VizHandle *> &list, QGraphicsItem *item) {
    list.append(new VizHandle(item, Kind::LEFT));
    list.append(new VizHandle(item, Kind::RIGHT));
    list.append(new VizHandle(item, Kind::TOP));
    list.append(new VizHandle(item, Kind::BOTTOM));
  }
};

#endif // VIZHANDLE_H
