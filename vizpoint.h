#ifndef VIZPOINT_H
#define VIZPOINT_H

#include <QGraphicsItem>
#include <QPainterPath>

#include <vizcoordinatesystem.h>
#include <vizpolyhedron.h>
#include <vizprogram.h>
#include <vizscop.h>
#include <vizstatement.h>

// FIXME: hardcoded value of distance between points.
#define VIZ_POINT_RADIUS   3
#define VIZ_POINT_DISTANCE 10

class VizPoint : public QGraphicsObject
{
  Q_OBJECT
public:
  const static int NO_COORD = INT_MAX;

  explicit VizPoint(QGraphicsItem *parent = 0);

  VizPolyhedron *polyhedron() const {
    return polyhedron_;
  }

  VizStatement *statement() const {
    return polyhedron_->statement();
  }

  VizScop *scop() const {
    return polyhedron_->scop();
  }

  VizProgram *program() const {
    return polyhedron_->program();
  }

  VizCoordinateSystem *coordinateSystem() const {
    return polyhedron_->coordinateSystem();
  }

  void setOriginalCoordinates(int horizontal = NO_COORD, int vertical = NO_COORD);
  void setScatteredCoordinates(int horizontal = NO_COORD, int vertical = NO_COORD);

  void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget);
  QRectF boundingRect() const;
  QPainterPath shape() const;

signals:

public slots:

private:
  VizPolyhedron *polyhedron_;
  QVector<int> alphaBetaVector;
  int m_originalHorizontal  = NO_COORD;
  int m_originalVertical    = NO_COORD;
  int m_scatteredHorizontal = NO_COORD;
  int m_scatteredVertical   = NO_COORD;
};

#endif // VIZPOINT_H
