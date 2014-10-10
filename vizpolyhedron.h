#ifndef VIZPOLYHEDRON_H
#define VIZPOLYHEDRON_H

#include <QGraphicsObject>
#include <QPainterPath>

#include "vizcoordinatesystem.h"
#include "clintprogram.h"
#include "clintscop.h"
#include "clintstmt.h"

class VizPoint;

class VizPolyhedron : public QGraphicsObject {
  Q_OBJECT
public:
  explicit VizPolyhedron(QGraphicsItem *parent = 0);

  ClintStmt *statement() const {
    return statement_;
  }

  VizCoordinateSystem *coordinateSystem() const {
    return coordinateSystem_;
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

  void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget);
  QRectF boundingRect() const;
  QPainterPath shape() const;

signals:

public slots:

private:
  ClintStmt *statement_;
  VizCoordinateSystem *coordinateSystem_;
  QPainterPath m_polyhedronShape;

  // TODO: introduce a QPolygon that corresponds to the convex hull for all the child points
  // update boundingRect and shape functions accordingly.

  std::unordered_set<VizPoint *> m_points;
  int m_localHorizontalMin = 0;
  int m_localVerticalMin   = 0;

  void setPointVisiblePos(VizPoint *vp, int x, int y);
  static std::pair<int, int> pointScatteredCoordsReal(const VizPoint *vp);
  std::vector<VizPoint *> convexHull() const;
  QPolygonF computePolygon() const;
  void recomputeShape();

  QPointF mapToCoordinates(double x, double y) const {
    return QPointF((x - m_localHorizontalMin) * VIZ_POINT_DISTANCE,
                   -(y - m_localVerticalMin) * VIZ_POINT_DISTANCE);
  }

  QPointF mapToCoordinates(std::pair<double, double> coords) const {
    return mapToCoordinates(coords.first, coords.second);
  }

  QPointF mapToCoordinates(VizPoint *vp) const {
    return mapToCoordinates(pointScatteredCoordsReal(vp));
  }
};

#endif // VIZPOLYHEDRON_H
