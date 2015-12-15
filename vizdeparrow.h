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
  int sourceInputDimensionality = sourcePolyhedron->occurrence()->inputDimensionality();
  int targetInputDimensionality = targetPolyhedron->occurrence()->inputDimensionality();

  typedef std::pair<std::pair<int, int>, std::pair<int, int>> DepCoordinates;
  std::unordered_set<DepCoordinates, boost::hash<DepCoordinates>> existingDependences;

  for (const std::vector<int> &dep : dependences) {
    CLINT_ASSERT(sourceInputDimensionality + targetInputDimensionality <= dep.size(),
                 "Not enough dimensions in a dependence projection");
    std::vector<int> sourceCoordinates(std::begin(dep),
                                       std::begin(dep) + sourceInputDimensionality);
    std::vector<int> targetCoordinates(std::begin(dep) + sourceInputDimensionality,
                                       std::begin(dep) + sourceInputDimensionality + targetInputDimensionality);

    VizPoint *sourcePoint = sourcePolyhedron->point(sourceCoordinates),
             *targetPoint = targetPolyhedron->point(targetCoordinates);

    if (!sourcePoint || !targetPoint)
      continue;

    DepCoordinates depCoordinates = std::make_pair(sourcePoint->scatteredCoordinates(),
                                                   targetPoint->scatteredCoordinates());

    // Omit self-dependences.
    if (depCoordinates.first == depCoordinates.second)
      continue;

    if (existingDependences.count(depCoordinates) == 0) {
      VizDepArrow *depArrow = new VizDepArrow(sourcePoint, targetPoint,
                                              parentObject, violated);
      if (violated) {
        depArrow->setZValue(100);
      } else {
        depArrow->setZValue(42);
      }
      set.insert(depArrow);
      existingDependences.emplace(depCoordinates);
    }
  }
}
#endif // VIZDEPARROW_H
