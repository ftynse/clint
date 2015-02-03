#include "macros.h"
#include "vizmanipulationmanager.h"
#include "vizpolyhedron.h"
#include "vizpoint.h"
#include "vizprojection.h"
#include "vizselectionmanager.h"
#include "clintdependence.h"

#include <QtGui>
#include <QtWidgets>

#include <algorithm>
#include <vector>

VizPolyhedron::VizPolyhedron(ClintStmtOccurrence *occurrence, VizCoordinateSystem *vcs) :
  QGraphicsObject(vcs), m_occurrence(occurrence), m_coordinateSystem(vcs) {

  setFlag(QGraphicsItem::ItemIsSelectable);
  setFlag(QGraphicsItem::ItemIsMovable);
  setFlag(QGraphicsItem::ItemSendsGeometryChanges);
  setAcceptHoverEvents(true);

  setOccurrenceSilent(occurrence);
}

void VizPolyhedron::updateHandlePositions() {
  if (m_handles.empty()) {
    m_handles = VizHandle::createCenterCornerHandles(this).toVector().toStdVector();
    setHandleVisible(false);

    for (VizHandle *vh : m_handles) {
      connect(vh, &VizHandle::moving, this, &VizPolyhedron::handleMoving);
      connect(vh, &VizHandle::aboutToMove, this, &VizPolyhedron::handleAboutToMove);
      connect(vh, &VizHandle::hasMoved, this, &VizPolyhedron::handleHasMoved);
    }
  } else {
    for (VizHandle *vh : m_handles) {
      vh->recomputePos();
    }
  }
}

void VizPolyhedron::setHandleVisible(bool visible) {
  for (VizHandle *vh : m_handles) {
    vh->setVisible(visible);
  }
}

void VizPolyhedron::setPointVisiblePos(VizPoint *vp, int x, int y) {
  const double pointDistance =
      m_coordinateSystem->projection()->vizProperties()->pointDistance();
  vp->setPos(x * pointDistance, -y * pointDistance);
}

void VizPolyhedron::setProjectedPoints(std::vector<std::vector<int>> &&points,
                                       int horizontalMin,
                                       int horiontalMax,
                                       int verticalMin,
                                       int verticalMax) {
  // FIXME: this function re-adds points if called twice; document or change this behavior
  CLINT_ASSERT(m_points.size() == 0, "Funciton may not be called twice for the same object");
  m_localHorizontalMin = horizontalMin;
  m_localHorizontalMax = horiontalMax;
  m_localVerticalMin   = verticalMin;
  m_localVerticalMax   = verticalMax;
  const VizProperties *props = coordinateSystem()->projection()->vizProperties();
  for (const std::vector<int> &point : points) {
    VizPoint *vp = new VizPoint(this);
    if (props->filledPoints()) {
      vp->setColor(props->color(occurrence()->betaVector()));
    } else {
      vp->setColor(QColor::fromRgb(100, 100, 100, 127));
    }

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
  recomputeShape();
  updateHandlePositions();
}

void VizPolyhedron::resetPointPositions() {
  for (VizPoint *vp : m_points) {
    int x, y;
    std::tie(x, y) = vp->scatteredCoordinates();
    if (x == VizPoint::NO_COORD) x = 0;
    if (y == VizPoint::NO_COORD) y = 0;
    setPointVisiblePos(vp, x - m_localHorizontalMin, y - m_localVerticalMin);
  }
}

void VizPolyhedron::occurrenceChanged() {
  int horizontalDim = coordinateSystem()->horizontalDimensionIdx();
  int verticalDim   = coordinateSystem()->verticalDimensionIdx();
  std::vector<std::vector<int>> points =
      occurrence()->projectOn(horizontalDim, verticalDim);
  m_localHorizontalMin = occurrence()->minimumValue(horizontalDim);
  m_localHorizontalMax = occurrence()->maximumValue(horizontalDim);
  m_localVerticalMin   = occurrence()->minimumValue(verticalDim);
  m_localVerticalMax   = occurrence()->maximumValue(verticalDim);

  double pointDistance = coordinateSystem()->projection()->vizProperties()->pointDistance();

  // We assume the number of points is the same?
  int unmatchedNewPoints = 0;
  std::unordered_set<VizPoint *> matchedPoints;
  for (const std::vector<int> &point : points) {
    std::pair<int, int> originalCoordinates  {VizPoint::NO_COORD, VizPoint::NO_COORD};
    std::pair<int, int> scatteredCoordinates {VizPoint::NO_COORD, VizPoint::NO_COORD};
    if (point.size() == 0) {
      CLINT_ASSERT(points.size() == 1, "Only one zero-dimensional point per polyhedron is allowed");
    } else if (point.size() == 2) {
      scatteredCoordinates.first = point[0];
      originalCoordinates.first = point[1];
    } else if (point.size() == 4) {
      scatteredCoordinates.first = point[0];
      scatteredCoordinates.second = point[1];
      originalCoordinates.first = point[2];
      originalCoordinates.second = point[3];
    } else {
      CLINT_UNREACHABLE;
    }
    VizPoint *vp = this->point(originalCoordinates);
    if (vp == nullptr) {
      ++unmatchedNewPoints;
    } else {
      vp->setScatteredCoordinates(scatteredCoordinates); // FIXME: no ifndef
      std::pair<int, int> realScatteredCoords = pointScatteredCoordsReal(vp);
      realScatteredCoords.first -= m_localHorizontalMin;
      realScatteredCoords.second -= m_localVerticalMin;
      QPointF position = vp->pos();
      position /= pointDistance;
      std::pair<int, int> visibleScatteredCoords {position.x(), -position.y()}; // inverted vertical axis
      // XXX: this was demoted to warning as it does not hold for grain (initially held for shift)
      CLINT_WARNING(realScatteredCoords == visibleScatteredCoords,
                   "Point projected to a different position than it is visible");
      matchedPoints.insert(vp);
    }
  }
  CLINT_ASSERT(unmatchedNewPoints == 0, "All new points should match old points by original coordinates");
  CLINT_ASSERT(matchedPoints.size() == m_points.size(), "Not all old points were matched");
  coordinateSystem()->polyhedronUpdated(this);
  updateHandlePositions();
}

VizPoint *VizPolyhedron::point(std::pair<int, int> &originalCoordinates) const {
  // FIXME: change this when points are stored as a map by original coordinates
  for (VizPoint *vp : m_points) {
    if (vp->originalCoordinates() == originalCoordinates) {
      return vp;
    }
  }
  return nullptr;
}

void VizPolyhedron::setInternalDependences(const std::vector<std::vector<int>> &dependences) {
  // Internal dependences are have even number of coordinates since dimensionality
  // of target and source domains are equal (they come from the same statement occurrence).
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

    // FIXME: this is very inefficient.  Change storage of points to a map by original coodinates.
    VizPoint *sourcePoint = nullptr,
             *targetPoint = nullptr;
    for (VizPoint *vp : m_points) {
      if (vp->originalCoordinates() == sourceCoordinates) {
        sourcePoint = vp;
      }
      if (vp->originalCoordinates() == targetCoordinates) {
        targetPoint = vp;
      }
      if (targetPoint && sourcePoint) {
        break;
      }
    }

    if (!targetPoint || !sourcePoint) {
//      qDebug() << "Could not find point corresponding to "
//               << sourceCoordinates.first << sourceCoordinates.second << " -> "
//               << targetCoordinates.first << targetCoordinates.second;
      continue;
    } else {

    }

    VizDepArrow *depArrow = new VizDepArrow(sourcePoint, targetPoint, this, false);
    m_deps.insert(depArrow);
  }
}

void VizPolyhedron::updateInternalDependences() {
  for (VizDepArrow *vda : m_deps) {
    vda->setParentItem(nullptr);
    vda->setVisible(false);
//    delete vda;
  }
  m_deps.clear();

  for (ClintDependence *dependence : m_occurrence->scop()->internalDependences(m_occurrence)) {
    setInternalDependences(dependence->projectOn(coordinateSystem()->horizontalDimensionIdx(),
                                                 coordinateSystem()->verticalDimensionIdx()));
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
  int commonX = pointScatteredCoordsReal(list.front()).first;
  int commonY = pointScatteredCoordsReal(list.front()).second;
  bool sameX = true,
       sameY = true;
  for (VizPoint *vp : list) {
    int x, y;
    std::tie(x, y) = pointScatteredCoordsReal(vp);
    sameX = sameX && x == commonX;
    sameY = sameY && y == commonY;
  }
  std::vector<VizPoint *>::iterator minIter, maxIter;
  if (sameX) {
    std::tie(minIter, maxIter) =
        std::minmax_element(std::begin(list), std::end(list), [](VizPoint *a, VizPoint *b){
      return pointScatteredCoordsReal(a).second < pointScatteredCoordsReal(b).second;
    });
  }
  if (sameY) {
    std::tie(minIter, maxIter) =
      std::minmax_element(std::begin(list), std::end(list), [](VizPoint *a, VizPoint *b){
      return pointScatteredCoordsReal(a).first < pointScatteredCoordsReal(b).first;
    });
  }
  if (sameX || sameY) {
    std::vector<VizPoint *> tempList {*minIter, *maxIter, *minIter};
    return std::move(tempList);
  }

  // Returns angle in range [0, 2*M_PI).
  static auto angle = [](const VizPoint *a, const VizPoint *b) -> double {
    int x0, y0, x1, y1;
    std::tie(x0, y0) = pointScatteredCoordsReal(a);
    std::tie(x1, y1) = pointScatteredCoordsReal(b);
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
    std::tie(x0, y0) = pointScatteredCoordsReal(a);
    std::tie(x1, y1) = pointScatteredCoordsReal(b);
    std::tie(x2, y2) = pointScatteredCoordsReal(c);
    return (x1 - x0) * (y2 - y0) - (y1 - y0) * (x2 - x0);
  };

  // Find the minimum Y-value point and put it to the first position.
  auto minYElement = std::min_element(std::begin(list),
                                      std::end(list),
                                      [](const VizPoint *a, const VizPoint *b) {
    int diff = pointScatteredCoordsReal(a).second - pointScatteredCoordsReal(b).second;
    if (diff == 0)
      return pointScatteredCoordsReal(a).first < pointScatteredCoordsReal(b).first;
    return diff < 0;
  });
  VizPoint *originPoint = *minYElement;
  std::iter_swap(std::begin(list), minYElement);

  // Sort the points by their angle to the minimum Y-value point.
  std::sort(std::begin(list) + 1, std::end(list), [originPoint](const VizPoint *a, const VizPoint *b) {
    double angleDiff = angle(originPoint, a) - angle(originPoint, b);
    if (fabs(angleDiff) < 1E-5) {
      int x0, y0, x1, y1, x2, y2;
      std::tie(x0, y0) = pointScatteredCoordsReal(a);
      std::tie(x1, y1) = pointScatteredCoordsReal(b);
      std::tie(x2, y2) = pointScatteredCoordsReal(originPoint);
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
  const double pointDistance =
      m_coordinateSystem->projection()->vizProperties()->pointDistance();
  std::vector<VizPoint *> points = convexHull();

  QPolygonF polygon;
  if (points.size() == 1) {
    int x, y;
    std::tie(x, y) = pointScatteredCoordsReal(points[0]);
    double x1 = x - m_localHorizontalMin + 0.5,
           x2 = x - m_localHorizontalMin - 0.5,
           y1 = -(y - m_localVerticalMin + 0.5),
           y2 = -(y - m_localVerticalMin - 0.5);
    polygon.append(QPointF(x1 * pointDistance, y1 * pointDistance));
    polygon.append(QPointF(x1 * pointDistance, y2 * pointDistance));
    polygon.append(QPointF(x2 * pointDistance, y2 * pointDistance));
    polygon.append(QPointF(x2 * pointDistance, y1 * pointDistance));
    polygon.append(QPointF(x1 * pointDistance, y1 * pointDistance));
    return polygon;
  }

  std::vector<std::pair<double, double>> polygonPoints;
  for (auto iter = std::begin(points); iter != std::end(points) - 1; ++iter) {
    VizPoint *p1 = *std::next(iter);
    VizPoint *p0 = *iter;
    int x0, y0, x1, y1;
    std::tie(x0, y0) = pointScatteredCoordsReal(p0);
    std::tie(x1, y1) = pointScatteredCoordsReal(p1);
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
    polygon.append(QPointF((point.first - m_localHorizontalMin) * pointDistance,
                           -(point.second - m_localVerticalMin) * pointDistance));
  }
  polygon.append(polygon.front());

  return polygon;
}

void VizPolyhedron::recomputeShape() {
  const double pointDistance =
      m_coordinateSystem->projection()->vizProperties()->pointDistance();
  std::vector<VizPoint *> points = convexHull();
  m_polyhedronShape = QPainterPath();

  // Special case for one-point polyhedron
  if (points.size() == 1) {
    QPointF center = mapToCoordinates(points.front());
    m_polyhedronShape.addRoundedRect(center.x() - pointDistance / 2.0,
                                     center.y() - pointDistance / 2.0,
                                     pointDistance,
                                     pointDistance,
                                     pointDistance / 4.0,
                                     pointDistance / 4.0);
    return;
  }
  // Even for a two-point polyhedron, convex hull would contain the first of them
  // twice since it is a closed polyg
  CLINT_ASSERT(points.size() >= 2, "Convex hull must have at least 3 points");

  std::vector<std::pair<double, double>> polygonPoints;
  for (auto iter = std::begin(points); iter != std::end(points) - 1; ++iter) {
    VizPoint *p1 = *std::next(iter);
    VizPoint *p0 = *iter;
    int x0, y0, x1, y1;
    std::tie(x0, y0) = pointScatteredCoordsReal(p0);
    std::tie(x1, y1) = pointScatteredCoordsReal(p1);
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

  m_polyhedronShape.moveTo(mapToCoordinates(polygonPoints.front()));
  polygonPoints.push_back(polygonPoints.front());
  for (size_t i = 1; i < polygonPoints.size() - 1; i += 2) {
    QPointF targetPoint = mapToCoordinates(polygonPoints[i]);
    QPointF nextPoint = mapToCoordinates(polygonPoints[i + 1]);
    m_polyhedronShape.lineTo(targetPoint);
    size_t centerIdx = (i + 1) / 2;
    int x, y;
    std::tie(x, y) = pointScatteredCoordsReal(points[centerIdx]);
    QPointF rotationCenter = mapToCoordinates(points[centerIdx]);
    QRectF arcRect(rotationCenter.x() - pointDistance / 2.0,
                   rotationCenter.y() - pointDistance / 2.0,
                   pointDistance, pointDistance);
    QLineF l1(rotationCenter, targetPoint);
    QLineF l2(rotationCenter, nextPoint);
    qreal angle = l1.angle();
    qreal length = l2.angle() - angle;
    // We know it is traversed CCW, then the angle has to be positive.
    if (length < 0)
      length += 360;
    m_polyhedronShape.arcTo(arcRect, angle, length);
  }
}

void VizPolyhedron::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) {
  Q_UNUSED(widget);
  painter->save();
  painter->setRenderHint(QPainter::Antialiasing);
  if (option->state & QStyle::State_Selected) {
    QPen fatPen = QPen(painter->pen());
    fatPen.setWidth(painter->pen().widthF() * 2.0);
    painter->setPen(fatPen);
  }
  painter->setBrush(m_backgroundColor);
  painter->drawPath(m_polyhedronShape);
  painter->restore();
}

QRectF VizPolyhedron::boundingRect() const {
  return m_polyhedronShape.boundingRect();
}

QPainterPath VizPolyhedron::shape() const {
  if (m_hovered) {
    QPainterPath path;
    path.addRect(m_polyhedronShape.boundingRect());
    return path;
  }
  return m_polyhedronShape;
}

QVariant VizPolyhedron::itemChange(GraphicsItemChange change, const QVariant &value) {
  if (change == QGraphicsItem::ItemSelectedHasChanged) {
    m_coordinateSystem->projection()->selectionManager()->polyhedronSelectionChanged(this, value.toBool());
  } else if (change == QGraphicsItem::ItemPositionHasChanged) {
    emit positionChanged();
  }
  return QGraphicsItem::itemChange(change, value);
}

void VizPolyhedron::mousePressEvent(QGraphicsSceneMouseEvent *event) {
  hideHandles();
  if (m_hovered && !m_polyhedronShape.contains(event->pos())) {
    return;
  }

  // Only left click.
  if (event->button() == Qt::LeftButton) {
    CLINT_ASSERT(m_wasPressed == false, "Button pressed twice without being released.");
    m_pressPos = pos();
    if (event->modifiers() & Qt::ShiftModifier) {
      m_wasShiftPressed = true;
      coordinateSystem()->projection()->manipulationManager()->polyhedronAboutToDetach(this);
    } else {
      m_wasPressed = true;
      coordinateSystem()->projection()->manipulationManager()->polyhedronAboutToMove(this);
    }
  }

  QGraphicsItem::mousePressEvent(event);
}

void VizPolyhedron::mouseMoveEvent(QGraphicsSceneMouseEvent *event) {
  if (m_hovered && !m_polyhedronShape.contains(event->pos())) {
    return;
  }

  QGraphicsItem::mouseMoveEvent(event);

  QPointF displacement = pos() - m_pressPos;
  VizManipulationManager *vmm = coordinateSystem()->projection()->manipulationManager();
  if (m_wasPressed) {
    vmm->polyhedronMoving(this, displacement);
  } else if (m_wasShiftPressed) {
    vmm->polyhedronDetaching(pos());
  }

}

void VizPolyhedron::mouseReleaseEvent(QGraphicsSceneMouseEvent *event) {
  if (m_hovered && !m_polyhedronShape.contains(event->pos())) {
    return;
  }

  if (event->button() == Qt::LeftButton) {
    CLINT_ASSERT(m_wasPressed || m_wasShiftPressed, "Button released without being pressed.");
    m_pressPos = QPointF(+0.0, +0.0);
    if (m_wasPressed) {
      m_wasPressed = false;
      coordinateSystem()->projection()->manipulationManager()->polyhedronHasMoved(this);
    } else if (m_wasShiftPressed) {
      m_wasShiftPressed = false;
      coordinateSystem()->projection()->manipulationManager()->polyhedronHasDetached(this);
    }
  }
  QGraphicsItem::mouseReleaseEvent(event);
}

void VizPolyhedron::hoverEnterEvent(QGraphicsSceneHoverEvent *event) {
  QGraphicsObject::hoverEnterEvent(event);
  if (!m_coordinateSystem->projection()->selectionManager()->selectedPoints().empty())
    return;
  setHandleVisible();
  m_hovered = true;
}

void VizPolyhedron::hoverLeaveEvent(QGraphicsSceneHoverEvent *event) {
  QGraphicsObject::hoverLeaveEvent(event);
  hideHandles();
}

std::pair<int, int> VizPolyhedron::pointScatteredCoordsReal(const VizPoint *vp) {
  std::pair<int, int> coords = vp->scatteredCoordinates();
  if (coords.first == VizPoint::NO_COORD) coords.first = 0;
  if (coords.second == VizPoint::NO_COORD) coords.second = 0;
  return std::move(coords);
}

QPointF VizPolyhedron::mapToCoordinates(double x, double y) const {
  const double pointDistance =
    m_coordinateSystem->projection()->vizProperties()->pointDistance();
  return QPointF((x - m_localHorizontalMin) * pointDistance,
                 -(y - m_localVerticalMin) * pointDistance);
}

void VizPolyhedron::reparentPoint(VizPoint *point) {
  if (point->polyhedron() == this)
    return;

  if (point->polyhedron())
    point->polyhedron()->m_points.erase(point);
  point->reparent(this);
  m_points.insert(point);
}

void VizPolyhedron::recomputeMinMax() {
  int horizontalMin = INT_MAX, horizontalMax = INT_MIN,
      verticalMin = INT_MAX, verticalMax = INT_MIN;
  for (VizPoint *vp : m_points) {
    int x,y;
    std::tie(x, y) = vp->scatteredCoordinates();
    if (x == VizPoint::NO_COORD) x = 0;
    if (y == VizPoint::NO_COORD) y = 0;
    horizontalMin = std::min(horizontalMin, x);
    horizontalMax = std::max(horizontalMax, x);
    verticalMin = std::min(verticalMin, y);
    verticalMax = std::max(verticalMax, y);
  }
  m_localHorizontalMin = horizontalMin;
  m_localHorizontalMax = horizontalMax;
  m_localVerticalMin = verticalMin;
  m_localVerticalMax = verticalMax;
}

void VizPolyhedron::prepareExtendRight(double extra) {
  VizProperties *props = m_coordinateSystem->projection()->vizProperties();
  const double pointDistance = props->pointDistance();

  // Update point positions.
  for (VizPoint *vp : m_points) {
    int xCoord, yCoord;
    std::tie(xCoord, yCoord) = vp->scatteredCoordinates();
    double ratioCoord = static_cast<double>(xCoord - m_localHorizontalMin) /
        static_cast<double>(m_localHorizontalMax - m_localHorizontalMin);
    double ratio = ratioCoord * (extra / pointDistance);
    vp->setPos(mapToCoordinates(xCoord + ratio, yCoord));
  }

  // Update polygonal shape positions (fast way without recomputing).
  // This will deform arcs around corners temporarily.
  const double xMax = (m_localHorizontalMax - m_localHorizontalMin) * pointDistance;
  for (int i = 0, e = m_originalPolyhedronShape.elementCount(); i < e; i++) {
    double x = m_originalPolyhedronShape.elementAt(i).x;
    double y = m_originalPolyhedronShape.elementAt(i).y;
    double ratio = x / xMax;
    x += extra * ratio;
    m_polyhedronShape.setElementPositionAt(i, x, y);
  }
  prepareGeometryChange();
}

void VizPolyhedron::prepareExtendLeft(double extra) {
  VizProperties *props = m_coordinateSystem->projection()->vizProperties();
  const double pointDistance = props->pointDistance();

  // Update point positions.
  for (VizPoint *vp : m_points) {
    int xCoord, yCoord;
    std::tie(xCoord, yCoord) = vp->scatteredCoordinates();
    double ratioCoord = static_cast<double>(m_localHorizontalMax - xCoord) /
        static_cast<double>(m_localHorizontalMax - m_localHorizontalMin);
    double ratio = ratioCoord * (extra / pointDistance);
    vp->setPos(mapToCoordinates(xCoord + ratio, yCoord));
  }

  // Update polygonal shape positions (fast way without recomputing).
  // This will deform arcs around corners temporarily.
  const double xMax = (m_localHorizontalMax - m_localHorizontalMin) * pointDistance;
  for (int i = 0, e = m_originalPolyhedronShape.elementCount(); i < e; i++) {
    double x = m_originalPolyhedronShape.elementAt(i).x;
    double y = m_originalPolyhedronShape.elementAt(i).y;
    double ratio = 1 - (x / xMax);
    x += extra * ratio;
    m_polyhedronShape.setElementPositionAt(i, x, y);
  }
  prepareGeometryChange();
}

void VizPolyhedron::prepareExtendUp(double extra) {
  VizProperties *props = m_coordinateSystem->projection()->vizProperties();
  const double pointDistance = props->pointDistance();

  // Update point positions.
  for (VizPoint *vp : m_points) {
    int xCoord, yCoord;
    std::tie(xCoord, yCoord) = vp->scatteredCoordinates();
    double ratioCoord = static_cast<double>(yCoord - m_localVerticalMin) /
        static_cast<double>(m_localVerticalMax - m_localVerticalMin);
    double ratio = ratioCoord * (extra / pointDistance);
    vp->setPos(mapToCoordinates(xCoord, yCoord + ratio));
  }

  // Update polygonal shape positions (fast way without recomputing).
  // This will deform arcs around corners temporarily.
  const double yMax = (m_localVerticalMax - m_localVerticalMin) * pointDistance;
  for (int i = 0, e = m_originalPolyhedronShape.elementCount(); i < e; i++) {
    double x = m_originalPolyhedronShape.elementAt(i).x;
    double y = m_originalPolyhedronShape.elementAt(i).y;
    double ratio = y / yMax;
    y += extra * ratio;
    m_polyhedronShape.setElementPositionAt(i, x, y);
  }
  prepareGeometryChange();
}

void VizPolyhedron::prepareExtendDown(double extra) {
  VizProperties *props = m_coordinateSystem->projection()->vizProperties();
  const double pointDistance = props->pointDistance();

  // Update point positions.
  for (VizPoint *vp : m_points) {
    int xCoord, yCoord;
    std::tie(xCoord, yCoord) = vp->scatteredCoordinates();
    double ratioCoord = static_cast<double>(m_localVerticalMax - yCoord) /
        static_cast<double>(m_localVerticalMax - m_localVerticalMin);
    double ratio = ratioCoord * (extra / pointDistance);
    vp->setPos(mapToCoordinates(xCoord, yCoord + ratio));
  }

  // Update polygonal shape positions (fast way without recomputing).
  // This will deform arcs around corners temporarily.
  const double yMax = -(m_localVerticalMax - m_localVerticalMin) * pointDistance;
  for (int i = 0, e = m_originalPolyhedronShape.elementCount(); i < e; i++) {
    double x = m_originalPolyhedronShape.elementAt(i).x;
    double y = m_originalPolyhedronShape.elementAt(i).y;
    double ratio = (y / yMax) - 1;
    y += extra * ratio;
    m_polyhedronShape.setElementPositionAt(i, x, y);
  }
  prepareGeometryChange();
}

void VizPolyhedron::prepareSkewVerticalRight(double offset) {
  VizProperties *props = m_coordinateSystem->projection()->vizProperties();
  const double pointDistance = props->pointDistance();

  // Update point positions.
  for (VizPoint *vp : m_points) {
    int xCoord, yCoord;
    std::tie(xCoord, yCoord) = vp->scatteredCoordinates();
//    double ratioCoord = static_cast<double>(yCoord - m_localVerticalMin + xCoord - m_localHorizontalMin) /
//        static_cast<double>(m_localVerticalMax - m_localVerticalMin + m_localHorizontalMax - m_localHorizontalMin);
    double ratioCoord = static_cast<double>(xCoord - m_localHorizontalMin) /
        static_cast<double>(m_localHorizontalMax - m_localHorizontalMin);
    double ratio = ratioCoord * (offset / pointDistance);
    vp->setPos(mapToCoordinates(xCoord, yCoord + ratio));
  }

  const double xMax = (m_localHorizontalMax - m_localHorizontalMin) * pointDistance;
  for (int i = 0, e = m_originalPolyhedronShape.elementCount(); i < e; i++) {
    double x = m_originalPolyhedronShape.elementAt(i).x;
    double y = m_originalPolyhedronShape.elementAt(i).y;
    double ratio = x / xMax;
    y -= offset * ratio;
    m_polyhedronShape.setElementPositionAt(i, x, y);
  }
  prepareGeometryChange();
}

void VizPolyhedron::prepareSkewVerticalLeft(double offset) {
  VizProperties *props = m_coordinateSystem->projection()->vizProperties();
  const double pointDistance = props->pointDistance();

  // Update point positions.
  for (VizPoint *vp : m_points) {
    int xCoord, yCoord;
    std::tie(xCoord, yCoord) = vp->scatteredCoordinates();
    double ratioCoord = static_cast<double>(m_localHorizontalMax - xCoord) /
        static_cast<double>(m_localHorizontalMax - m_localHorizontalMin);
    double ratio = ratioCoord * (offset / pointDistance);
    vp->setPos(mapToCoordinates(xCoord, yCoord + ratio));
  }

  const double xMax = (m_localHorizontalMax - m_localHorizontalMin) * pointDistance;
  for (int i = 0, e = m_originalPolyhedronShape.elementCount(); i < e; i++) {
    double x = m_originalPolyhedronShape.elementAt(i).x;
    double y = m_originalPolyhedronShape.elementAt(i).y;
    double ratio = 1 - x / xMax;
    y -= offset * ratio;
    m_polyhedronShape.setElementPositionAt(i, x, y);
  }
  prepareGeometryChange();
}

void VizPolyhedron::prepareSkewHorizontalTop(double offset) {
  VizProperties *props = m_coordinateSystem->projection()->vizProperties();
  const double pointDistance = props->pointDistance();

  // Update point positions.
  for (VizPoint *vp : m_points) {
    int xCoord, yCoord;
    std::tie(xCoord, yCoord) = vp->scatteredCoordinates();
    double ratioCoord = static_cast<double>(yCoord - m_localVerticalMin) /
        static_cast<double>(m_localVerticalMax - m_localVerticalMin);
    double ratio = ratioCoord * (offset / pointDistance);
    vp->setPos(mapToCoordinates(xCoord + ratio, yCoord));
  }

  const double yMax = (m_localVerticalMax - m_localVerticalMin) * pointDistance;
  for (int i = 0, e = m_originalPolyhedronShape.elementCount(); i < e; i++) {
    double x = m_originalPolyhedronShape.elementAt(i).x;
    double y = m_originalPolyhedronShape.elementAt(i).y;
    double ratio = y / yMax;
    x -= offset * ratio;
    m_polyhedronShape.setElementPositionAt(i, x, y);
  }
  prepareGeometryChange();
}

void VizPolyhedron::prepareSkewHorizontalBottom(double offset) {
  VizProperties *props = m_coordinateSystem->projection()->vizProperties();
  const double pointDistance = props->pointDistance();

  // Update point positions.
  for (VizPoint *vp : m_points) {
    int xCoord, yCoord;
    std::tie(xCoord, yCoord) = vp->scatteredCoordinates();
    double ratioCoord = static_cast<double>(m_localVerticalMax - yCoord) /
        static_cast<double>(m_localVerticalMax - m_localVerticalMin);
    double ratio = ratioCoord * (offset / pointDistance);
    vp->setPos(mapToCoordinates(xCoord + ratio, yCoord));
  }

  const double yMax = (m_localVerticalMax - m_localVerticalMin) * pointDistance;
  for (int i = 0, e = m_originalPolyhedronShape.elementCount(); i < e; i++) {
    double x = m_originalPolyhedronShape.elementAt(i).x;
    double y = m_originalPolyhedronShape.elementAt(i).y;
    double ratio = 1 + y / yMax;
    x += offset * ratio;
    m_polyhedronShape.setElementPositionAt(i, x, y);
  }
  prepareGeometryChange();
}

void VizPolyhedron::handleAboutToMove(const VizHandle *const handle) {
  m_coordinateSystem->projection()->selectionManager()->clearSelection();

  VizManipulationManager *vmm = m_coordinateSystem->projection()->manipulationManager();

  m_originalPolyhedronShape = m_polyhedronShape;
  switch (handle->kind()) {
  case VizHandle::Kind::RIGHT:
    vmm->polyhedronAboutToResize(this, VizManipulationManager::Dir::RIGHT);
    break;
  case VizHandle::Kind::LEFT:
    vmm->polyhedronAboutToResize(this, VizManipulationManager::Dir::LEFT);
    break;
  case VizHandle::Kind::TOP:
    vmm->polyhedronAboutToResize(this, VizManipulationManager::Dir::UP);
    break;
  case VizHandle::Kind::BOTTOM:
    vmm->polyhedronAboutToResize(this, VizManipulationManager::Dir::DOWN);
    break;
  case VizHandle::Kind::TOPRIGHT:
    vmm->polyhedronAboutToSkew(this, VizManipulationManager::C_TOP | VizManipulationManager::C_RIGHT);
    break;
  case VizHandle::Kind::BOTTOMRIGHT:
    vmm->polyhedronAboutToSkew(this, VizManipulationManager::C_BOTTOM | VizManipulationManager::C_RIGHT);
    break;
  case VizHandle::Kind::TOPLEFT:
    vmm->polyhedronAboutToSkew(this, VizManipulationManager::C_TOP | VizManipulationManager::C_LEFT);
    break;
  case VizHandle::Kind::BOTTOMLEFT:
    vmm->polyhedronAboutToSkew(this, VizManipulationManager::C_BOTTOM | VizManipulationManager::C_LEFT);
    break;
  case VizHandle::Kind::NB_KINDS:
    CLINT_UNREACHABLE;
  }
}

void VizPolyhedron::handleMoving(const VizHandle *const handle, QPointF displacement) {
  VizManipulationManager *vmm = m_coordinateSystem->projection()->manipulationManager();
  switch (handle->kind()) {
  case VizHandle::Kind::RIGHT:
  case VizHandle::Kind::LEFT:
  case VizHandle::Kind::TOP:
  case VizHandle::Kind::BOTTOM:
    vmm->polyhedronResizing(displacement);
    break;
  case VizHandle::Kind::TOPRIGHT:
  case VizHandle::Kind::BOTTOMRIGHT:
  case VizHandle::Kind::TOPLEFT:
  case VizHandle::Kind::BOTTOMLEFT:
    // Do not allow simultaneous horizontal and vertical skew (no support in clay)
    if (fabs(displacement.x()) < 5 || fabs(displacement.y()) > fabs(displacement.x()))
      displacement.rx() = 0;
    else
      displacement.ry() = 0;
    vmm->polyhedronSkewing(displacement);
    break;
  default:
    break;
  }
}

void VizPolyhedron::handleHasMoved(const VizHandle *const handle, QPointF displacement) {
  prepareGeometryChange();
  VizManipulationManager *vmm = m_coordinateSystem->projection()->manipulationManager();
  switch (handle->kind()) {
  case VizHandle::Kind::RIGHT:
  case VizHandle::Kind::LEFT:
  case VizHandle::Kind::TOP:
  case VizHandle::Kind::BOTTOM:
    vmm->polyhedronHasResized(this);
    break;
  case VizHandle::Kind::TOPRIGHT:
  case VizHandle::Kind::BOTTOMRIGHT:
  case VizHandle::Kind::TOPLEFT:
  case VizHandle::Kind::BOTTOMLEFT:
    vmm->polyhedronHasSkewed(this);
    break;
  default:
    CLINT_UNREACHABLE;
  }
  updateHandlePositions();
}

void VizPolyhedron::setOccurrenceSilent(ClintStmtOccurrence *occurrence) {
  if (m_occurrence) {
    disconnect(occurrence, &ClintStmtOccurrence::pointsChanged, this, &VizPolyhedron::occurrenceChanged);
  }
  m_occurrence = occurrence;
  if (occurrence) {
    m_backgroundColor = m_coordinateSystem->projection()->vizProperties()->color(occurrence->betaVector());
    connect(occurrence, &ClintStmtOccurrence::pointsChanged, this, &VizPolyhedron::occurrenceChanged);
  }
}
