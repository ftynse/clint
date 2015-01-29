#ifndef VIZPOLYHEDRON_H
#define VIZPOLYHEDRON_H

#include <QGraphicsObject>
#include <QPainterPath>

#include "vizcoordinatesystem.h"
#include "vizdeparrow.h"
#include "vizhandle.h"
#include "clintprogram.h"
#include "clintscop.h"
#include "clintstmt.h"
#include "clintstmtoccurrence.h"

#include <QtWidgets>
#include <QtGui>

class VizPoint;

class VizPolyhedron : public QGraphicsObject {
  Q_OBJECT
public:
  explicit VizPolyhedron(ClintStmtOccurrence *occurrence, VizCoordinateSystem *vcs);

  ClintStmtOccurrence *occurrence() const {
    return m_occurrence;
  }

  ClintStmt *statement() const {
    return m_occurrence->statement();
  }

  VizCoordinateSystem *coordinateSystem() const {
    return m_coordinateSystem;
  }

  ClintScop *scop() const {
    return m_occurrence->scop();
  }

  ClintProgram *program() const {
    return m_occurrence->program();
  }

  int localHorizontalMin() const {
    return m_localHorizontalMin;
  }

  int localVerticalMin() const {
    return m_localVerticalMin;
  }

  int localHorizontalMax() const {
    return m_localHorizontalMax;
  }

  int localVerticalMax() const {
    return m_localVerticalMax;
  }

  void recomputeMinMax();

  void setProjectedPoints(std::vector<std::vector<int>> &&points,
                          int horizontalMin, int horizontalMax,
                          int verticalMin, int verticalMax);
  void setInternalDependences(const std::vector<std::vector<int>> &dependences);
  void resetPointPositions();

  void reparent(VizCoordinateSystem *vcs) {
    if (vcs == m_coordinateSystem)
      return;
    m_coordinateSystem = vcs;
    QPointF scenePosition = scenePos();
    setParentItem(vcs);
    setPos(vcs->mapFromScene(scenePosition));
  }

  void setOverrideSetPos(bool value = true) {
    m_overrideSetPos = value;
  }

  void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget);
  QRectF boundingRect() const;
  QPainterPath shape() const;
  QVariant itemChange(GraphicsItemChange change, const QVariant &value);
  void mousePressEvent(QGraphicsSceneMouseEvent *event);
  void mouseMoveEvent(QGraphicsSceneMouseEvent *event);
  void mouseReleaseEvent(QGraphicsSceneMouseEvent *event);
  void hoverEnterEvent(QGraphicsSceneHoverEvent *event);
  void hoverLeaveEvent(QGraphicsSceneHoverEvent *event);

  QColor color() const {
    return m_backgroundColor;
  }

  void setColor(QColor clr) {
    m_backgroundColor = clr;
    update();
  }

  void setPos(const QPointF &position) {
    if (m_overrideSetPos) {
      m_pressPos = position;
    } else {
      QGraphicsObject::setPos(position);
    }
  }
  void setPos(qreal x, qreal y) {
    setPos(QPointF(x, y));
  }

  VizPoint *point(std::pair<int, int> &originalCoordinates) const;

  void reparentPoint(VizPoint *point);

  void updateShape() {
    recomputeMinMax();
    recomputeShape();
    resetPointPositions();
  }

  void updateInternalDependences();

  void setOccurrenceSilent(ClintStmtOccurrence *occurrence);

  void hideHandles() {
    setHandleVisible(false);
    m_hovered = false;
  }

signals:
  void positionChanged();

public slots:
  void occurrenceChanged();

private:
  ClintStmtOccurrence *m_occurrence;
  VizCoordinateSystem *m_coordinateSystem;
  QPainterPath m_polyhedronShape;
  QColor m_backgroundColor;

  std::vector<VizHandle *> m_handles;
  bool m_hovered = false;

  std::unordered_set<VizPoint *> m_points;
  std::unordered_set<VizDepArrow *> m_deps;
  int m_localHorizontalMin = 0;
  int m_localVerticalMin   = 0;
  int m_localHorizontalMax = 0;
  int m_localVerticalMax   = 0;

  bool m_selectionChangeBarrier = false;

  bool m_wasPressed = false;
  bool m_wasShiftPressed = false;
  bool m_overrideSetPos = false;
  QPointF m_pressPos;

  void setPointVisiblePos(VizPoint *vp, int x, int y);
  static std::pair<int, int> pointScatteredCoordsReal(const VizPoint *vp);
  std::vector<VizPoint *> convexHull() const;
  QPolygonF computePolygon() const;
  void recomputeShape();

  void updateHandlePositions();
  void setHandleVisible(bool visible = true);

  QPointF mapToCoordinates(double x, double y) const;

  QPointF mapToCoordinates(std::pair<double, double> coords) const {
    return mapToCoordinates(coords.first, coords.second);
  }

  QPointF mapToCoordinates(VizPoint *vp) const {
    return mapToCoordinates(pointScatteredCoordsReal(vp));
  }
};

#endif // VIZPOLYHEDRON_H
