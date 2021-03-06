#ifndef VIZPOLYHEDRON_H
#define VIZPOLYHEDRON_H

#include "clintprogram.h"
#include "clintscop.h"
#include "clintstmt.h"
#include "clintstmtoccurrence.h"
#include "vizcoordinatesystem.h"
#include "vizhandle.h"

#include <QGraphicsObject>
#include <QPainterPath>
#include <QtWidgets>
#include <QtGui>

#include <unordered_map>

#include <boost/functional/hash.hpp>

class VizPoint;
class VizDepArrow;

class VizPolyhedron : public QGraphicsObject {
  Q_OBJECT
public:
  explicit VizPolyhedron(ClintStmtOccurrence *occurrence, VizCoordinateSystem *vcs);
  ~VizPolyhedron();

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

  void setInternalDependences(std::vector<std::vector<int>> &&dependences);
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

  VizPoint *point(const std::vector<int> &originalCoordinates) const;
  std::unordered_set<VizPoint *> points() const;

  void reparentPoint(VizPoint *point);
  bool hasPoints() const;

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

  void prepareExtendRight(double extra);
  void prepareExtendLeft(double extra);
  void prepareExtendUp(double extra);
  void prepareExtendDown(double extra);

  void prepareSkewHorizontalTop(double offset);
  void prepareSkewHorizontalBottom(double offset);
  void prepareSkewVerticalLeft(double offset);
  void prepareSkewVerticalRight(double offset);

  double prepareRotate(QPointF displacement);
  void resetRotate();

  void prepareRotateAngle(double angle);

  void debugPrintPoints();
signals:
  void positionChanged();

public slots:
  void occurrenceChanged();
  void handleMoving(const VizHandle *const handle, QPointF displacement);
  void handleAboutToMove(const VizHandle *const handle);
  void handleHasMoved(const VizHandle *const handle, QPointF displacement);

  void occurrenceDeleted();

private:
  ClintStmtOccurrence *m_occurrence = nullptr;
  VizCoordinateSystem *m_coordinateSystem;
  QPainterPath m_polyhedronShape;
  QColor m_backgroundColor;

  std::vector<VizHandle *> m_handles;
  bool m_hovered = false;
  QPainterPath m_originalPolyhedronShape;

  std::vector<QLineF> m_tileLines;

  // Mapping from 2D original coordinates to all points in the projection with these original coordinates.
  typedef std::unordered_map<std::vector<int>, VizPoint *, boost::hash<std::vector<int>>> PointMap;
  PointMap m_pts;
  PointMap m_pointOthers; /// Points with original coordinates different than those in m_pts, projected at the same position.
  void reprojectPoints();

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

  QPointF m_rotationCenter;

  bool m_mouseEventForwarding = false;

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
  void disconnectAll();
};

#endif // VIZPOLYHEDRON_H
