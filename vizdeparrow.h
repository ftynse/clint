#ifndef VIZDEPARROW_H
#define VIZDEPARROW_H

#include <QGraphicsItem>
#include <QLineF>
#include <QPainterPath>

class VizDepArrow : public QGraphicsItem {
public:
  VizDepArrow(QGraphicsItem *parent = nullptr);

  void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget);
  QRectF boundingRect() const;

  static VizDepArrow *pointLink(QPointF source, QPointF target);
private:
  QLineF m_arrowLine;
  QPainterPath m_arrowHead;
};

#endif // VIZDEPARROW_H
