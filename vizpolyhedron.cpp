#include "macros.h"
#include "vizpolyhedron.h"
#include "vizpoint.h"

#include <QtGui>
#include <QtWidgets>

#include <algorithm>
#include <vector>

VizPolyhedron::VizPolyhedron(QGraphicsItem *parent) :
  QGraphicsObject(parent) {
}

void VizPolyhedron::setPointVisiblePos(VizPoint *vp, int x, int y) {
  vp->setPos(x * VIZ_POINT_DISTANCE, -y * VIZ_POINT_DISTANCE);
}

void VizPolyhedron::setProjectedPoints(std::vector<std::vector<int>> &&points,
                                       int horizontalMin,
                                       int verticalMin) {
  m_localHorizontalMin = horizontalMin;
  m_localVerticalMin   = verticalMin;
  for (const std::vector<int> &point : points) {
    VizPoint *vp = new VizPoint(this);
    if (point.size() == 0) {
      vp->setScatteredCoordinates();
      vp->setOriginalCoordinates();
      setPointVisiblePos(vp, 0, 0);
    } else if (point.size() == 2) {
      vp->setOriginalCoordinates(point[1]);
      vp->setScatteredCoordinates(point[0]);
      setPointVisiblePos(vp, point[0] - horizontalMin, 0);
    } else if (point.size() == 4) {
      vp->setOriginalCoordinates(point[2], point[3]);
      vp->setScatteredCoordinates(point[0], point[1]);
      setPointVisiblePos(vp, point[0] - horizontalMin, point[1] - verticalMin);
    } else {
      CLINT_ASSERT(!"unreachable", "Point has wrong number of dimensions");
    }
    m_points.insert(vp);
    // TODO: setup alpha-beta vector for point
    // alphas that are not in the current projection are wildcards for this point, can't get them
    // Or it can be reconstructed using respective parent polyhedron and coodrinate system
  }
}

std::vector<VizPoint *> VizPolyhedron::convexHull() const {
  std::vector<VizPoint *> list;
  for (VizPoint *vp : m_points) {
    list.push_back(vp);
  }

  CLINT_ASSERT(list.size() != 0,
               "Trying to compute the convex hull for the empty polyhedron");

  // One point is its own convex hull.
  if (list.size() == 1)
    return std::move(list);

  // Check if points form a single line.
  // Either one-dimensional projection of the loop, or loop with one iteration.
  int commonX = list.front()->scatteredCoordinates().first;
  int commonY = list.front()->scatteredCoordinates().second;
  bool sameX = true,
       sameY = true;
  for (VizPoint *vp : list) {
    int x, y;
    std::tie(x, y) = vp->scatteredCoordinates();
    sameX = sameX && x == commonX;
    sameY = sameY && y == commonY;
  }
  std::vector<VizPoint *>::iterator minIter, maxIter;
  if (sameX) {
    std::tie(minIter, maxIter) =
        std::minmax_element(std::begin(list), std::end(list), [](VizPoint *a, VizPoint *b){
      return a->scatteredCoordinates().second < b->scatteredCoordinates().second;
    });
  }
  if (sameY) {
    std::tie(minIter, maxIter) =
      std::minmax_element(std::begin(list), std::end(list), [](VizPoint *a, VizPoint *b){
      return a->scatteredCoordinates().first < b->scatteredCoordinates().first;
    });
  }
  if (sameX || sameY) {
    std::vector<VizPoint *> tempList {*minIter, *maxIter, *minIter};
    return std::move(tempList);
  }

  // Returns angle in range [0, 2*M_PI).
  static auto angle = [](const VizPoint *a, const VizPoint *b) -> double {
    int x0, y0, x1, y1;
    std::tie(x0, y0) = a->scatteredCoordinates();
    std::tie(x1, y1) = b->scatteredCoordinates();
    int A = (y1 - y0);
    int B = (x1 - x0);
    double phi = std::atan2(A, B);
    if (phi < 0)
      phi = 2 * M_PI - phi;
    return phi;
  };

  // Cross-product
  static auto ccw = [](const VizPoint *a, const VizPoint *b, const VizPoint *c) -> int {
    int x0, y0, x1, y1, x2, y2;
    std::tie(x0, y0) = a->scatteredCoordinates();
    std::tie(x1, y1) = b->scatteredCoordinates();
    std::tie(x2, y2) = c->scatteredCoordinates();
    return (x1 - x0) * (y2 - y0) - (y1 - y0) * (x2 - x0);
  };

  // Find the minimum Y-value point and put it to the first position.
  auto minYElement = std::min_element(std::begin(list),
                                      std::end(list),
                                      [](const VizPoint *a, const VizPoint *b) {
    int diff = a->scatteredCoordinates().second - b->scatteredCoordinates().second;
    if (diff == 0)
      return a->scatteredCoordinates().first < b->scatteredCoordinates().first;
    return diff < 0;
  });
  VizPoint *originPoint = *minYElement;
  std::iter_swap(std::begin(list), minYElement);

  // Sort the points by their angle to the minimum Y-value point.
  std::sort(std::begin(list) + 1, std::end(list), [originPoint](const VizPoint *a, const VizPoint *b) {
    double angleDiff = angle(originPoint, a) - angle(originPoint, b);
    if (fabs(angleDiff) < 1E-5) {
      int x0, y0, x1, y1, x2, y2;
      std::tie(x0, y0) = a->scatteredCoordinates();
      std::tie(x1, y1) = b->scatteredCoordinates();
      std::tie(x2, y2) = originPoint->scatteredCoordinates();
      return sqrt((x0 - x2) * (x0 - x2) + (y0 - y2) * (y0 - y2)) <
             sqrt((x1 - x2) * (x1 - x2) + (y1 - y2) * (y1 - y2));
    }
    return angleDiff < 0;
  });

  // Graham scan
  list.insert(std::begin(list), list.back());
  auto convexHullIter = std::begin(list) + 1;
  auto listBegin = std::begin(list);
  auto listEnd = std::end(list);
  for (auto iter = listBegin + 2; iter != listEnd; ++iter) {
    while (ccw(*std::prev(convexHullIter), *convexHullIter, *iter) <= 0) {
      if (std::distance(listBegin, convexHullIter) > 1)
        --convexHullIter;
      else if (iter == std::prev(listEnd))
        break;
      else
        ++iter;
    }
    ++convexHullIter;
    std::iter_swap(iter, convexHullIter);
  }
  list.erase(std::next(convexHullIter), std::end(list));
  list.erase(std::begin(list));
  list.push_back(list.front());

  return std::move(list);
}

QPolygonF VizPolyhedron::computePolygon() const {
  std::vector<VizPoint *> points = convexHull();

  QPolygonF polygon;
  if (points.size() == 1) {
    int x, y;
    std::tie(x, y) = points[0]->scatteredCoordinates();
    double x1 = x - m_localHorizontalMin + 0.5,
           x2 = x - m_localHorizontalMin - 0.5,
           y1 = -(y - m_localVerticalMin + 0.5),
           y2 = -(y - m_localVerticalMin - 0.5);
    polygon.append(QPointF(x1 * VIZ_POINT_DISTANCE, y1 * VIZ_POINT_DISTANCE));
    polygon.append(QPointF(x1 * VIZ_POINT_DISTANCE, y2 * VIZ_POINT_DISTANCE));
    polygon.append(QPointF(x2 * VIZ_POINT_DISTANCE, y2 * VIZ_POINT_DISTANCE));
    polygon.append(QPointF(x2 * VIZ_POINT_DISTANCE, y1 * VIZ_POINT_DISTANCE));
    polygon.append(QPointF(x1 * VIZ_POINT_DISTANCE, y1 * VIZ_POINT_DISTANCE));
    return polygon;
  }

  std::vector<std::pair<double, double>> polygonPoints;
  for (auto iter = std::begin(points); iter != std::end(points) - 1; ++iter) {
    VizPoint *p1 = *std::next(iter);
    VizPoint *p0 = *iter;
    int x0, y0, x1, y1;
    std::tie(x0, y0) = p0->scatteredCoordinates();
    std::tie(x1, y1) = p1->scatteredCoordinates();
    int vecX = x1 - x0;
    int vecY = y1 - y0;
    // Rotate 90 degrees clockwise
    // [x']   [cos -90  -sin -90] [x]   [ 0  1] [x]   [ y]
    // [  ] = [                 ] [ ] = [     ] [ ] = [  ]
    // [y']   [sin -90   cos -90] [y]   [-1  0] [y]   [-x]
    // Normalize (divide by length) to unit vector, and take half of that.
    double length = std::sqrt(vecX*vecX + vecY*vecY);
    double offsetX = vecY / length / 2.0;
    double offsetY = -vecX / length / 2.0;
    polygonPoints.emplace_back(std::make_pair(x0 + offsetX, y0 + offsetY));
    polygonPoints.emplace_back(std::make_pair(x1 + offsetX, y1 + offsetY));
  }

  for (const std::pair<double, double> &point : polygonPoints) {
    polygon.append(QPointF((point.first - m_localHorizontalMin) * VIZ_POINT_DISTANCE,
                           -(point.second - m_localVerticalMin) * VIZ_POINT_DISTANCE));
  }
  polygon.append(polygon.front());

  return polygon;
}

void VizPolyhedron::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) {
  Q_UNUSED(option);
  Q_UNUSED(widget);
  painter->save();
  painter->drawPolygon(computePolygon());
  painter->restore();
}

QRectF VizPolyhedron::boundingRect() const {
  return computePolygon().boundingRect();
}

QPainterPath VizPolyhedron::shape() const {
  return QGraphicsItem::shape();
}

std::pair<int, int> VizPolyhedron::pointScatteredCoordsReal(VizPoint *vp) {
  std::pair<int, int> coords = vp->scatteredCoordinates();
  if (coords.first == VizPoint::NO_COORD) coords.first = 0;
  if (coords.second == VizPoint::NO_COORD) coords.second = 0;
  return std::move(coords);
}
