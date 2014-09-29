#ifndef VIZPOLYHEDRON_H
#define VIZPOLYHEDRON_H

#include <QGraphicsObject>
#include <QPainterPath>

#include <vizcoordinatesystem.h>
#include <vizprogram.h>
#include <vizscop.h>
#include <vizstatement.h>

class VizPoint;

class VizPolyhedron : public QGraphicsObject {
  Q_OBJECT
public:
  explicit VizPolyhedron(QGraphicsItem *parent = 0);

  VizStatement *statement() const {
    return statement_;
  }

  VizCoordinateSystem *coordinateSystem() const {
    return coordinateSystem_;
  }

  VizScop *scop() const {
    return statement_->scop();
  }

  VizProgram *program() const {
    return statement_->program();
  }

  void setupFromPoints__(std::vector<std::vector<int>> &&points);

  void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget);
  QRectF boundingRect() const;
  QPainterPath shape() const;

signals:

public slots:

private:
  VizStatement *statement_;
  VizCoordinateSystem *coordinateSystem_;

  // TODO: introduce a QPolygon that corresponds to the convex hull for all the child points
  // update boundingRect and shape functions accordingly.

  QSet<VizPoint *> points_;
};

#endif // VIZPOLYHEDRON_H
