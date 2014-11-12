#ifndef VIZPOLYHEDRON_H
#define VIZPOLYHEDRON_H

#include <QGraphicsObject>
#include <QPainterPath>

#include "vizcoordinatesystem.h"
#include "vizdeparrow.h"
#include "clintprogram.h"
#include "clintscop.h"
#include "clintstmt.h"

#include <QtWidgets>
#include <QtGui>

class VizPoint;

class VizPolyhedron : public QGraphicsObject {
  Q_OBJECT
public:
  explicit VizPolyhedron(VizCoordinateSystem *vcs);

  ClintStmt *statement() const {
    return statement_;
  }

  VizCoordinateSystem *coordinateSystem() const {
    return m_coordinateSystem;
  }

  ClintScop *scop() const {
    return statement_->scop();
  }

  ClintProgram *program() const {
    return statement_->program();
  }

  int localHorizontalMin() const {
    return m_localHorizontalMin;
  }

  int localVerticalMin() const {
    return m_localVerticalMin;
  }

  void setProjectedPoints(std::vector<std::vector<int>> &&points, int horizontalMin, int verticalMin);
  void setInternalDependences(const std::vector<std::vector<int>> &dependences);

  void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget);
  QRectF boundingRect() const;
  QPainterPath shape() const;
  QVariant itemChange(GraphicsItemChange change, const QVariant &value);

signals:

public slots:

private:
  ClintStmt *statement_;
  VizCoordinateSystem *m_coordinateSystem;
  QPainterPath m_polyhedronShape;

  std::unordered_set<VizPoint *> m_points;
  std::unordered_set<VizDepArrow *> m_deps;
  int m_localHorizontalMin = 0;
  int m_localVerticalMin   = 0;

  bool m_selectionChangeBarrier = false;

  void setPointVisiblePos(VizPoint *vp, int x, int y);
  static std::pair<int, int> pointScatteredCoordsReal(const VizPoint *vp);
  std::vector<VizPoint *> convexHull() const;
  QPolygonF computePolygon() const;
  void recomputeShape();

  QPointF mapToCoordinates(double x, double y) const;

  QPointF mapToCoordinates(std::pair<double, double> coords) const {
    return mapToCoordinates(coords.first, coords.second);
  }

  QPointF mapToCoordinates(VizPoint *vp) const {
    return mapToCoordinates(pointScatteredCoordsReal(vp));
  }
};

#endif // VIZPOLYHEDRON_H
