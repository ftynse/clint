#ifndef VIZPOINT_H
#define VIZPOINT_H

#include <QGraphicsItem>
#include <QPainterPath>

#include <vizcoordinatesystem.h>
#include <vizpolyhedron.h>
#include <vizprogram.h>
#include <vizscop.h>
#include <vizstatement.h>

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

  void setOriginalCoordinates(const std::pair<int, int> &coordinates) {
    setOriginalCoordinates(coordinates.first, coordinates.second);
  }

  void setScatteredCoordinates(const std::pair<int, int> &coordinates) {
    setScatteredCoordinates(coordinates.first, coordinates.second);
  }

  std::pair<int, int> originalCoordinates() const {
    return std::move(std::make_pair(m_originalHorizontal, m_originalVertical));
  }

  std::pair<int, int> scatteredCoordinates() const {
    return std::move(std::make_pair(m_scatteredHorizontal, m_scatteredVertical));
  }

  void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget);
  QRectF boundingRect() const;
  QPainterPath shape() const;

signals:

public slots:

private:
  VizPolyhedron *polyhedron_;
  QVector<int> alphaBetaVector;
  // These coordinates do not define point position on the coordinate system
  // or in the polyhedron.  The position is relative to the axes intersection
  // and minima of the point coordinates in the polyhedron.
  int m_originalHorizontal  = NO_COORD;
  int m_originalVertical    = NO_COORD;
  int m_scatteredHorizontal = NO_COORD;
  int m_scatteredVertical   = NO_COORD;
};

#endif // VIZPOINT_H
