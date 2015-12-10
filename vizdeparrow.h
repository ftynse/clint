#ifndef VIZDEPARROW_H
#define VIZDEPARROW_H

#include <QGraphicsItem>
#include <QLineF>
#include <QPainterPath>

#include "vizproperties.h"
#include "vizpoint.h"
#include "vizpolyhedron.h"

#include <unordered_set>

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

template <typename ParentType>
void vizDependenceArrowsCreate(VizPolyhedron *sourcePolyhedron,
                               VizPolyhedron *targetPolyhedron,
                               std::vector<std::vector<int> > &&dependences,
                               bool violated,
                               ParentType *parentObject,
                               std::unordered_set<VizDepArrow *> &set) {
  // one CS is not supposed to contain polyhedra of different visible dimensionality, so number of coordinates are equal
  for (const std::vector<int> &dep : dependences) {
    std::pair<int, int> sourceCoordinates {VizPoint::NO_COORD, VizPoint::NO_COORD};
    std::pair<int, int> targetCoordinates {VizPoint::NO_COORD, VizPoint::NO_COORD};
    if (dep.size() == 0) {
      // TODO: figure out what to do with such dependence
      continue;
    } else if (dep.size() == 2) {
      sourceCoordinates.first = dep[0];
      targetCoordinates.first = dep[1];
    } else if (dep.size() == 4) {
      sourceCoordinates.first = dep[0];
      sourceCoordinates.second = dep[1];
      targetCoordinates.first = dep[2];
      targetCoordinates.second = dep[3];
    } else {
      CLINT_UNREACHABLE;
    }

    std::unordered_set<VizPoint *> sourcePoints = sourcePolyhedron->points(sourceCoordinates);
    std::unordered_set<VizPoint *> targetPoints = targetPolyhedron->points(targetCoordinates);

    if (targetPoints.size() == 0 || sourcePoints.size() == 0) {
      continue;
    }

    // XXX: this may be wrong when projecting skewed 3d on 2d: we assume every dependence between
    // original points should be kept for each pair of scattered points with identical original coordinates,
    // but it may be false.  Use both original and scattered coordinates in the dependence to select only
    // those truly present for the point?  Will index-set splitting fix it automatically?
    for (VizPoint *sourcePoint : sourcePoints) {
      for (VizPoint *targetPoint : targetPoints) {
        VizDepArrow *depArrow = new VizDepArrow(sourcePoint, targetPoint,
                                                parentObject, violated);
        if (violated) {
          depArrow->setZValue(100);
        } else {
          depArrow->setZValue(42);
        }
        set.insert(depArrow);
      }
    }
  }
}
#endif // VIZDEPARROW_H
