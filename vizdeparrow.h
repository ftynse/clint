#ifndef VIZDEPARROW_H
#define VIZDEPARROW_H

#include <QGraphicsItem>
#include <QLineF>
#include <QPainterPath>

#include "vizproperties.h"

class VizPolyhedron;
class VizCoordinateSystem;
class VizPoint;

class VizDepArrow : public QGraphicsObject {
public:
  VizDepArrow(QPointF source, QPointF target, VizPolyhedron *parent, bool violated);
  VizDepArrow(QPointF source, QPointF target, VizCoordinateSystem *parent, bool violated);

  VizDepArrow(VizPoint *source, VizPoint *target, VizPolyhedron *parent, bool violated);
  VizDepArrow(VizPoint *source, VizPoint *target, VizCoordinateSystem *parent, bool violated);

  void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget);
  QRectF boundingRect() const;

private slots:
  void repoint();

private:
  void pointLink(QPointF source, QPointF target);
  void pointLinkCS(VizPoint *source, VizPoint *target);

  VizPoint *m_sourcePoint = nullptr,
           *m_targetPoint = nullptr;
  VizCoordinateSystem *m_coordinateSystemParent = nullptr;

  bool m_violated;
  QLineF m_arrowLine;
  QPainterPath m_arrowHead;
  const VizProperties *m_vizProperties;
};

#endif // VIZDEPARROW_H
