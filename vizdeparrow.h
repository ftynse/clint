#ifndef VIZDEPARROW_H
#define VIZDEPARROW_H

#include <QGraphicsItem>
#include <QLineF>
#include <QPainterPath>

#include "vizproperties.h"

class VizPolyhedron;
class VizCoordinateSystem;

class VizDepArrow : public QGraphicsItem {
public:
  VizDepArrow(QPointF source, QPointF target, VizPolyhedron *parent, bool violated);
  VizDepArrow(QPointF source, QPointF target, VizCoordinateSystem *parent, bool violated);

  void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget);
  QRectF boundingRect() const;

private:
  void pointLink(QPointF source, QPointF target);

  bool m_violated;
  QLineF m_arrowLine;
  QPainterPath m_arrowHead;
  const VizProperties *m_vizProperties;
};

#endif // VIZDEPARROW_H
