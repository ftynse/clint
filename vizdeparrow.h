#ifndef VIZDEPARROW_H
#define VIZDEPARROW_H

#include <QGraphicsItem>
#include <QLineF>
#include <QPainterPath>

#include "vizproperties.h"

class VizPolyhedron;

class VizDepArrow : public QGraphicsItem {
public:
  VizDepArrow(QPointF source, QPointF target, QGraphicsItem *parent = nullptr);

  void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget);
  QRectF boundingRect() const;

private:
  void pointLink(QPointF source, QPointF target);

  QLineF m_arrowLine;
  QPainterPath m_arrowHead;
  VizPolyhedron *m_polyhedron;
};

#endif // VIZDEPARROW_H
