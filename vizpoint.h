#ifndef VIZPOINT_H
#define VIZPOINT_H

#include <QGraphicsItem>
#include <QPainterPath>

#include <vizcoordinatesystem.h>
#include <vizpolyhedron.h>
#include <clintprogram.h>
#include <clintscop.h>
#include <clintstmt.h>

class VizPoint : public QGraphicsObject
{
  Q_OBJECT
public:
  const static int NO_COORD = INT_MAX;

  explicit VizPoint(VizPolyhedron *polyhedron);

  VizPolyhedron *polyhedron() const {
    return m_polyhedron;
  }

  ClintStmt *statement() const {
    return m_polyhedron->statement();
  }

  ClintScop *scop() const {
    return m_polyhedron->scop();
  }

  ClintProgram *program() const {
    return m_polyhedron->program();
  }

  VizCoordinateSystem *coordinateSystem() const {
    return m_polyhedron->coordinateSystem();
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
  QVariant itemChange(GraphicsItemChange change, const QVariant &value);

  void setColor(QColor color) {
    m_color = color;
  }

signals:

public slots:

private:
  VizPolyhedron *m_polyhedron;
  QVector<int> alphaBetaVector;
  // These coordinates do not define point position on the coordinate system
  // or in the polyhedron.  The position is relative to the axes intersection
  // and minima of the point coordinates in the polyhedron.
  int m_originalHorizontal  = NO_COORD;
  int m_originalVertical    = NO_COORD;
  int m_scatteredHorizontal = NO_COORD;
  int m_scatteredVertical   = NO_COORD;

  bool m_selectionChangeBarrier = false;

  QColor m_color;
};

#endif // VIZPOINT_H
