#include "macros.h"
#include "vizdeparrow.h"
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

VizPolyhedron::VizPolyhedron(ClintStmtOccurrence *occurrence, VizCoordinateSystem *vcs, CreateShadowTag) :
  QGraphicsObject(vcs), m_occurrence(occurrence), m_coordinateSystem(vcs) {

  setEnabled(false);
  setOccurrenceImmediate(occurrence);
  disconnectAll();
}

VizPolyhedron::VizPolyhedron(ClintStmtOccurrence *occurrence, VizCoordinateSystem *vcs) :
  QGraphicsObject(vcs), m_occurrence(occurrence), m_coordinateSystem(vcs) {

  setFlag(QGraphicsItem::ItemIsSelectable);
  setFlag(QGraphicsItem::ItemIsMovable);
  setFlag(QGraphicsItem::ItemSendsGeometryChanges);
  setAcceptHoverEvents(true);

  m_shapeAnimation = new VizPolyhedronShapeAnimation(this);
  connect(m_shapeAnimation, &VizPolyhedronShapeAnimation::finished, this, &VizPolyhedron::updateHandlePositionsOnFinished);

  setOccurrenceImmediate(occurrence);
}

VizPolyhedron::~VizPolyhedron() {
  disconnectAll();
  // Delete deps before points (all of them are children of this VizPolyhedron, but deps depend on
  // points to exist).
  for (VizDepArrow *vda : m_deps) {
    vda->setParentItem(nullptr);
    delete vda;
  }
  m_deps.clear();
}

VizPolyhedron *VizPolyhedron::createShadow(bool visible) {
  VizPolyhedron *shadow = new VizPolyhedron(m_occurrence, m_coordinateSystem, CreateShadowTag());
  shadow->setVisible(visible);
  return shadow;
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

void VizPolyhedron::updateHandlePositionsOnFinished() {
  if (!m_disableHandleUpdatesInAnimation) {
    m_disableHandleUpdatesInAnimation = true;
    updateHandlePositions();
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

void VizPolyhedron::resetPointPositions() {
  for (auto p : m_pts) {
    VizPoint *vp = p.second;
    int x, y;
    std::tie(x, y) = vp->scatteredCoordinates();
    if (x == VizPoint::NO_COORD) x = 0;
    if (y == VizPoint::NO_COORD) y = 0;
    setPointVisiblePos(vp, x, y);
  }
}

bool VizPolyhedron::hasPoints() const {
  return m_pts.size() != 0;
}

void VizPolyhedron::reprojectPoints() {
  int horizontalDim = coordinateSystem()->horizontalDimensionIdx();
  int verticalDim   = coordinateSystem()->verticalDimensionIdx();
  CLINT_ASSERT(m_occurrence, "empty occurrence changed");
  std::vector<std::vector<int>> points =
      occurrence()->projectOn(horizontalDim, verticalDim);

  PointMap updatedPoints, extraPoints;
  std::unordered_map<std::pair<int, int>,
                     VizPoint *,
                     boost::hash<std::pair<int, int>>> visibleScatteredCoordiantes;

  for (const std::vector<int> &point : points) {
    std::pair<int, int> scatteredCoordinates;
    std::vector<int> originalCoordinates;
    std::tie(originalCoordinates, scatteredCoordinates) =
        m_occurrence->parseProjectedPoint(point,
                                          coordinateSystem()->horizontalDimensionIdx(),
                                          coordinateSystem()->verticalDimensionIdx());
    if (visibleScatteredCoordiantes.count(scatteredCoordinates) == 0) {
      VizPoint *vp;
      auto it = m_pts.find(originalCoordinates);
      if (it != m_pts.end()) {
        vp = it->second;
        m_pts.erase(it);
      } else {
        vp = new VizPoint(this);
        vp->setOriginalCoordinates(originalCoordinates);
      }
      vp->setScatteredCoordinates(scatteredCoordinates);
      updatedPoints.emplace(originalCoordinates, vp);
      visibleScatteredCoordiantes.emplace(scatteredCoordinates, vp);
    } else {
      VizPoint *vp = visibleScatteredCoordiantes[scatteredCoordinates];
      extraPoints.emplace(originalCoordinates, vp);
    }
  }

  for (auto p : m_pts) {
    p.second->setParent(nullptr);
    p.second->setParentItem(nullptr);
    p.second->deleteLater();
  }
  m_pts.clear();
  m_pts = updatedPoints;
  m_pointOthers.clear();
  m_pointOthers = extraPoints;
}

void VizPolyhedron::enlargeCoordinateSystem() {
  VizProjection *projection = coordinateSystem()->projection();
  int horizontalDim = coordinateSystem()->horizontalDimensionIdx();
  int verticalDim   = coordinateSystem()->verticalDimensionIdx();

  int horizontalMin = m_occurrence->minimumValue(horizontalDim);
  int horizontalMax = m_occurrence->maximumValue(horizontalDim);
  int verticalMin = m_occurrence->minimumValue(verticalDim);
  int verticalMax = m_occurrence->maximumValue(verticalDim);
  projection->ensureFitsHorizontally(coordinateSystem(), horizontalMin, horizontalMax);
  projection->ensureFitsVertically(coordinateSystem(), verticalMin, verticalMax);
}

void VizPolyhedron::setupAnimation() {
  if (m_transitionAnimation && m_transitionAnimation->state() == QAbstractAnimation::Running) {
    CLINT_DEBUG(true, "Running a second animation before the previous one stopped");
  }

  // TODO: animation for cases where points are created or removed?

  const double pointDistance =
      m_coordinateSystem->projection()->vizProperties()->pointDistance();

//  int oldLocalHorizontalMin = m_localHorizontalMin;
//  int oldLocalVerticalMin = m_localVerticalMin;
  recomputeMinMax(); // FIXME: is this necessary now?
//  double horizontalOffset = (m_localHorizontalMin - oldLocalHorizontalMin) * pointDistance;
//  double verticalOffset = (m_localVerticalMin - oldLocalVerticalMin) * pointDistance;

  int animationDuration = coordinateSystem()->projection()->vizProperties()->animationDuration();
  QParallelAnimationGroup *group = new QParallelAnimationGroup();
  for (auto p : m_pts) {
    VizPoint *vp = p.second;
    QPropertyAnimation *animation = new QPropertyAnimation(vp, "pos", group);
    int x, y;
    std::tie(x, y) = vp->scatteredCoordinates();
    if (x == VizPoint::NO_COORD) x = 0;
    if (y == VizPoint::NO_COORD) y = 0;
//    x -= m_localHorizontalMin;
//    y -= m_localVerticalMin;
    QPointF targetPos = QPointF(x * pointDistance, -y * pointDistance);
    QPointF currentPos = vp->pos();
//    currentPos += QPointF(horizontalOffset, verticalOffset);
    animation->setDuration(animationDuration);
    animation->setStartValue(currentPos);
    animation->setEndValue(targetPos);
    group->addAnimation(animation);
  }

  m_shapeAnimation->setDuration(animationDuration);
  group->addAnimation(m_shapeAnimation);

  group->start();
  group->pause();

  m_transitionAnimation = group;
}

void VizPolyhedron::playAnimation() {
  if (m_transitionAnimation->state() == QAbstractAnimation::Paused)
    m_transitionAnimation->resume();
  else
    m_transitionAnimation->start();
}

void VizPolyhedron::setAnimationProgress(double progress) {
  int currentTime = progress * m_transitionAnimation->duration();
  m_transitionAnimation->setCurrentTime(currentTime);
}

void VizPolyhedron::stopAnimation() {
  m_transitionAnimation->stop();
}

void VizPolyhedron::clearAnimation() {
  if (m_transitionAnimation) {
    stopAnimation();
    if (m_shapeAnimation)
      m_shapeAnimation->setParent(nullptr);
    delete m_transitionAnimation;
    m_transitionAnimation = nullptr;
  }
}

// Create lines that symbolize tiles.
void VizPolyhedron::recreateTileLines() {
  int horizontalDim = coordinateSystem()->horizontalDimensionIdx();
  int verticalDim   = coordinateSystem()->verticalDimensionIdx();

  VizProperties *props = coordinateSystem()->projection()->vizProperties();
  m_tileLines.clear();
  int horizontalScatDim = 2 * horizontalDim + 1;
  int verticalScatDim   = 2 * verticalDim + 1;
  // We assume one dimension in only tiled one time (which is consitent with the current implementaiton).
  if (horizontalDim != VizProperties::NO_DIMENSION &&
      m_occurrence->isTiled(horizontalScatDim)) {
    int tileSize = m_occurrence->tileSize(m_occurrence->ignoreTilingDim(horizontalScatDim) - 2);
    CLINT_ASSERT(tileSize != 0,
                 "Dimension appears to be tiled, but the tile size is zero");
    for (int h = (m_localHorizontalMin / tileSize + 1) * tileSize; h <= m_localHorizontalMax; h += tileSize) {
      m_tileLines.emplace_back((h - 0.5) * props->pointDistance(),
                               -(-0.5) * props->pointDistance(),
                               (h - 0.5) * props->pointDistance(),
                               -(m_localVerticalMax - m_localVerticalMin + 0.5) * props->pointDistance());
    }
  }
  if (verticalDim != VizProperties::NO_DIMENSION &&
      m_occurrence->isTiled(verticalScatDim)) {
    int tileSize = m_occurrence->tileSize(m_occurrence->ignoreTilingDim(verticalScatDim) - 2);
    CLINT_ASSERT(tileSize != 0,
                 "Dimension appears to be tiled, but the tile size is zero");
    for (int v = (m_localVerticalMin / tileSize + 1) * tileSize; v <= m_localVerticalMax; v += tileSize) {
      m_tileLines.emplace_back((-0.5) * props->pointDistance(),
                               -(v - 0.5) * props->pointDistance(),
                               (m_localHorizontalMax - m_localHorizontalMin + 0.5) * props->pointDistance(),
                               -(v - 0.5) * props->pointDistance());
    }
  }
}

void VizPolyhedron::skipNextUpdate() {
  m_skipNextUpdate = true;
}

void VizPolyhedron::finalizeOccurrenceChange() {
  m_disableHandleUpdatesInAnimation = false;
  playAnimation();

  m_coordinateSystem->projection()->ensureFitsHorizontally(
        m_coordinateSystem, localHorizontalMin(), localHorizontalMax());
  m_coordinateSystem->projection()->ensureFitsVertically(
        m_coordinateSystem, localVerticalMin(), localVerticalMax());

  recreateTileLines();

  coordinateSystem()->polyhedronUpdated(this);
  updateHandlePositions();
}

void VizPolyhedron::occurrenceChanged() {
  CLINT_ASSERT(m_occurrence, "empty occurrence changed");

  reprojectPoints();
  if (m_skipNextUpdate) {
    m_skipNextUpdate = false;
    return;
  }

  m_disableHandleUpdatesInAnimation = false;
  setupAnimation();
  playAnimation();
  m_coordinateSystem->projection()->ensureFitsHorizontally(
        m_coordinateSystem, localHorizontalMin(), localHorizontalMax());
  m_coordinateSystem->projection()->ensureFitsVertically(
        m_coordinateSystem, localVerticalMin(), localVerticalMax());

  recreateTileLines();

  coordinateSystem()->polyhedronUpdated(this);
  updateHandlePositions();
}

std::unordered_set<VizPoint *> VizPolyhedron::points() const {
  std::unordered_set<VizPoint *> result;
  for (auto it = std::begin(m_pts); it != std::end(m_pts); ++it) {
    result.insert(it->second);
  }
  return std::move(result);
}

VizPoint *VizPolyhedron::point(const std::vector<int> &originalCoordinates) const {
  if (m_pts.count(originalCoordinates) != 0) {
    return m_pts.at(originalCoordinates);
  } else if (m_pointOthers.count(originalCoordinates) != 0) {
    return m_pointOthers.at(originalCoordinates);
  } else {
    return nullptr;
  }
}

void VizPolyhedron::setInternalDependences(std::vector<std::vector<int>> &&dependences) {
  vizDependenceArrowsCreate(this, this, std::forward<std::vector<std::vector<int>>>(dependences), false, this, m_deps);
}

void VizPolyhedron::updateInternalDependences() {
  prepareGeometryChange();
  for (VizDepArrow *vda : m_deps) {
    // The geometry change scheduled above needs the size of all "old" objects to properly
    // compute the rectangle it updates.  Therefore, VizDepArrows should not be deleted
    // immediately, but scheduled for deletion after change.
    vda->setVisible(false);
    vda->deleteLater();
  }
  m_deps.clear();

  for (ClintDependence *dependence : m_occurrence->scop()->internalDependences(m_occurrence)) {
    setInternalDependences(dependence->projectOn(coordinateSystem()->horizontalDimensionIdx(),
                                                 coordinateSystem()->verticalDimensionIdx()));
  }
}

static std::pair<int, int> vizPointPosPair(const VizPoint *vp) {
  QPointF posf = vp->pos();
  QPoint pos = posf.toPoint();
  return std::make_pair(pos.x(), -pos.y());
}

static QPointF vizPointPos(const VizPoint *vp) {
  return vp->pos();
}

static QPointF vizPointPosNeg(const VizPoint *vp) {
  return QPointF(vp->pos().x(), -vp->pos().y());
}

std::vector<VizPoint *> VizPolyhedron::convexHull() const {
  // Use []() { return vp->scatteredCoordiantes(); } for non-visual based convex hull
  return convexHullImpl2(vizPointPosNeg);
}

std::vector<VizPoint *> VizPolyhedron::convexHullImpl2(std::function<QPointF (const VizPoint *)> coordinates) const {
  std::vector<VizPoint *> list;
  for (auto p : m_pts) {
    VizPoint *vp = p.second;
    list.push_back(vp);
  }

  CLINT_ASSERT(list.size() != 0,
               "Trying to compute the convex hull for the empty polyhedron");

  // One point is its own convex hull.
  if (list.size() == 1)
    return std::move(list);

  // Check if points form a single line.
  // Either one-dimensional projection of the loop, or loop with one iteration.
  QPointF commonPoint = coordinates(list.front());
  int commonX = static_cast<int>(round(commonPoint.x()));
  int commonY = static_cast<int>(round(commonPoint.y()));
  bool sameX = true,
       sameY = true;
  for (VizPoint *vp : list) {
    QPointF point = coordinates(vp);
    int x, y;
    x = static_cast<int>(round(point.x()));
    y = static_cast<int>(round(point.y()));
    sameX = sameX && x == commonX;
    sameY = sameY && y == commonY;
  }
  std::vector<VizPoint *>::iterator minIter, maxIter;
  if (sameX) {
    std::tie(minIter, maxIter) =
        std::minmax_element(std::begin(list), std::end(list), [coordinates](VizPoint *a, VizPoint *b){
      return coordinates(a).y() < coordinates(b).y();
    });
  }
  if (sameY) {
    std::tie(minIter, maxIter) =
      std::minmax_element(std::begin(list), std::end(list), [coordinates](VizPoint *a, VizPoint *b){
      return coordinates(a).x() < coordinates(b).x();
    });
  }
  if (sameX || sameY) {
    std::vector<VizPoint *> tempList {*minIter, *maxIter, *minIter};
    return std::move(tempList);
  }

  // Returns angle in range [0, 2*M_PI).
  static auto angle = [coordinates](const VizPoint *a, const VizPoint *b) -> double {
    double x0, y0, x1, y1;
    QPointF p0 = coordinates(a);
    QPointF p1 = coordinates(b);
    x0 = p0.x();
    x1 = p1.x();
    y0 = p0.y();
    y1 = p1.y();
    double A = (y1 - y0);
    double B = (x1 - x0);
    double phi = std::atan2(A, B);
    if (phi < 0)
      phi = 2 * M_PI - phi;
    return phi;
  };

  // Cross-product
  static auto ccw = [coordinates](const VizPoint *a, const VizPoint *b, const VizPoint *c) -> int {
    double x0, y0, x1, y1, x2, y2;
    QPointF p0 = coordinates(a);
    QPointF p1 = coordinates(b);
    QPointF p2 = coordinates(c);
    x0 = p0.x();
    x1 = p1.x();
    x2 = p2.x();
    y0 = p0.y();
    y1 = p1.y();
    y2 = p2.y();
    return (x1 - x0) * (y2 - y0) - (y1 - y0) * (x2 - x0);
  };

  // Find the minimum Y-value point and put it to the first position.
  auto minYElement = std::min_element(std::begin(list),
                                      std::end(list),
                                      [coordinates](const VizPoint *a, const VizPoint *b) {
    double diff = coordinates(a).y() - coordinates(b).y();
    if (diff == 0)
      return coordinates(a).x() < coordinates(b).x();
    return diff < 0;
  });
  VizPoint *originPoint = *minYElement;
  std::iter_swap(std::begin(list), minYElement);

  // Sort the points by their angle to the minimum Y-value point.
  std::sort(std::begin(list) + 1, std::end(list), [originPoint,coordinates](const VizPoint *a, const VizPoint *b) {
    double angleDiff = angle(originPoint, a) - angle(originPoint, b);
    if (fabs(angleDiff) < 1E-5) {
    double x0, y0, x1, y1, x2, y2;
    QPointF p0 = coordinates(a);
    QPointF p1 = coordinates(b);
    QPointF p2 = coordinates(originPoint);
    x0 = p0.x();
    x1 = p1.x();
    x2 = p2.x();
    y0 = p0.y();
    y1 = p1.y();
    y2 = p2.y();
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

std::vector<VizPoint *> VizPolyhedron::convexHullImpl(std::function<std::pair<int, int> (const VizPoint *)> coordinates) const {
  std::vector<VizPoint *> list;
  for (auto p : m_pts) {
    VizPoint *vp = p.second;
    list.push_back(vp);
  }

  CLINT_ASSERT(list.size() != 0,
               "Trying to compute the convex hull for the empty polyhedron");

  // One point is its own convex hull.
  if (list.size() == 1)
    return std::move(list);

  // Check if points form a single line.
  // Either one-dimensional projection of the loop, or loop with one iteration.
  int commonX = coordinates(list.front()).first;
  int commonY = coordinates(list.front()).second;
  bool sameX = true,
       sameY = true;
  for (VizPoint *vp : list) {
    int x, y;
    std::tie(x, y) = coordinates(vp);
    sameX = sameX && x == commonX;
    sameY = sameY && y == commonY;
  }
  std::vector<VizPoint *>::iterator minIter, maxIter;
  if (sameX) {
    std::tie(minIter, maxIter) =
        std::minmax_element(std::begin(list), std::end(list), [coordinates](VizPoint *a, VizPoint *b){
      return coordinates(a).second < coordinates(b).second;
    });
  }
  if (sameY) {
    std::tie(minIter, maxIter) =
      std::minmax_element(std::begin(list), std::end(list), [coordinates](VizPoint *a, VizPoint *b){
      return coordinates(a).first < coordinates(b).first;
    });
  }
  if (sameX || sameY) {
    std::vector<VizPoint *> tempList {*minIter, *maxIter, *minIter};
    return std::move(tempList);
  }

  // Returns angle in range [0, 2*M_PI).
  static auto angle = [coordinates](const VizPoint *a, const VizPoint *b) -> double {
    int x0, y0, x1, y1;
    std::tie(x0, y0) = coordinates(a);
    std::tie(x1, y1) = coordinates(b);
    int A = (y1 - y0);
    int B = (x1 - x0);
    double phi = std::atan2(A, B);
    if (phi < 0)
      phi = 2 * M_PI - phi;
    return phi;
  };

  // Cross-product
  static auto ccw = [coordinates](const VizPoint *a, const VizPoint *b, const VizPoint *c) -> int {
    int x0, y0, x1, y1, x2, y2;
    std::tie(x0, y0) = coordinates(a);
    std::tie(x1, y1) = coordinates(b);
    std::tie(x2, y2) = coordinates(c);
    return (x1 - x0) * (y2 - y0) - (y1 - y0) * (x2 - x0);
  };

  // Find the minimum Y-value point and put it to the first position.
  auto minYElement = std::min_element(std::begin(list),
                                      std::end(list),
                                      [coordinates](const VizPoint *a, const VizPoint *b) {
    int diff = coordinates(a).second - coordinates(b).second;
    if (diff == 0)
      return coordinates(a).first < coordinates(b).first;
    return diff < 0;
  });
  VizPoint *originPoint = *minYElement;
  std::iter_swap(std::begin(list), minYElement);

  // Sort the points by their angle to the minimum Y-value point.
  std::sort(std::begin(list) + 1, std::end(list), [originPoint,coordinates](const VizPoint *a, const VizPoint *b) {
    double angleDiff = angle(originPoint, a) - angle(originPoint, b);
    if (fabs(angleDiff) < 1E-5) {
      int x0, y0, x1, y1, x2, y2;
      std::tie(x0, y0) = coordinates(a);
      std::tie(x1, y1) = coordinates(b);
      std::tie(x2, y2) = coordinates(originPoint);
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

QPolygonF VizPolyhedron::recomputePolygon() const {
  // Use pointScatteredCoordsReal * pointDistance for non-visual polygon
  return recomputePolygonImpl(vizPointPos);
}

QPolygonF VizPolyhedron::recomputePolygonImpl(std::function<QPointF (const VizPoint *)> coordinates) const {
  const double pointDistance =
      m_coordinateSystem->projection()->vizProperties()->pointDistance();
  std::vector<VizPoint *> points = convexHull();

  QPolygonF polygon;
  if (points.size() == 1) {
    QPointF p = coordinates(points[0]);
    p.rx() -= pointDistance / 2;
    p.ry() -= pointDistance / 2;
    polygon.append(p);
    p.rx() += pointDistance;
    polygon.append(p);
    p.ry() += pointDistance;
    polygon.append(p);
    p.rx() -= pointDistance;
    polygon.append(p);
    p.ry() -= pointDistance;
    polygon.append(p);

    return polygon;
  }

  std::vector<QPointF> polygonPoints = buildPolygonPoints(points, coordinates);

  for (QPointF point : polygonPoints) {
    polygon.append(point);
  }

  polygon.append(polygon.front());

  return polygon;
}

std::vector<QPointF>
VizPolyhedron::buildPolygonPoints(std::vector<VizPoint *> points,
                                  std::function<QPointF (const VizPoint *)> coordinates) const {
  const double pointDistance =
      m_coordinateSystem->projection()->vizProperties()->pointDistance();

  std::vector<QPointF> polygonPoints;
  for (auto iter = std::begin(points); iter != std::end(points) - 1; ++iter) {
    VizPoint *p1 = *std::next(iter);
    VizPoint *p0 = *iter;

    QPointF qp0 = coordinates(p0);
    QPointF qp1 = coordinates(p1);
    double x0 = qp0.x();
    double y0 = qp0.y();
    double x1 = qp1.x();
    double y1 = qp1.y();

    int vecX = x1 - x0;
    int vecY = y1 - y0;
    // Treat the line as vector (in CCW order), find a normal to it (90º rotation) with
    // rotation matrix
    // [x']   [cos 90  -sin 90] [x]   [ 0 -1] [x]   [-y]
    // [  ] = [               ] [ ] = [     ] [ ] = [  ]
    // [y']   [sin 90   cos 90] [y]   [ 1  0] [y]   [ x]
    // Renormalize the length of the new vector to (pointDistance / 2).
    double length = std::sqrt(vecX*vecX + vecY*vecY);
    double offsetX = length == 0.0 ? 0.0 : -vecY / length * pointDistance / 2;
    double offsetY = length == 0.0 ? 0.0 : vecX / length * pointDistance / 2;
    polygonPoints.emplace_back(QPointF(x0 + offsetX, y0 + offsetY));
    polygonPoints.emplace_back(QPointF(x1 + offsetX, y1 + offsetY));
  }

  return polygonPoints;
}

void VizPolyhedron::recomputeShape() {
  // Use pointScatteredCoordsReal * pointDistance for non-visual polygon
  recomputeSmoothShapeImpl(vizPointPos);
  prepareGeometryChange();
}

void VizPolyhedron::recomputeSmoothShapeImpl(std::function<QPointF (const VizPoint *)> coordinates) {
  const double pointDistance =
      m_coordinateSystem->projection()->vizProperties()->pointDistance();
  std::vector<VizPoint *> points = convexHull();
  m_polyhedronShape = QPainterPath();

  // Special case for one-point polyhedron
  if (points.size() == 1) {
    QPointF center = coordinates(points.front());
    m_polyhedronShape.addRoundedRect(center.x() - pointDistance / 2.0,
                                     center.y() - pointDistance / 2.0,
                                     pointDistance,
                                     pointDistance,
                                     pointDistance / 4.0,
                                     pointDistance / 4.0);
    return;
  }
  // Even for a two-point polyhedron, convex hull would contain the first of them
  // twice since it is a closed polygon.
  CLINT_ASSERT(points.size() >= 2, "Convex hull must have at least 3 points");

  // Create angle points of the visible polygon.
  std::vector<QPointF> polygonPoints = buildPolygonPoints(points, coordinates);

  // Draw a polygon with rounded angles.
  m_polyhedronShape.moveTo(polygonPoints.front());
  polygonPoints.push_back(polygonPoints.front());
  for (size_t i = 1; i < polygonPoints.size() - 1; i += 2) {
    QPointF targetPoint = polygonPoints[i];
    QPointF nextPoint = polygonPoints[i + 1];
    m_polyhedronShape.lineTo(targetPoint);
    size_t centerIdx = (i + 1) / 2;
    QPointF rotationCenter = coordinates(points[centerIdx]);
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
  if (!(option->state & QStyle::State_Enabled)) {
    QPen grayPen = QPen(painter->pen());
    grayPen.setColor(Qt::gray);
    painter->setPen(grayPen);
  } else if (option->state & QStyle::State_Selected) {
    QPen fatPen = QPen(painter->pen());
    fatPen.setWidth(painter->pen().widthF() * 2.0);
    painter->setPen(fatPen);
  }

  if (option->state & QStyle::State_Enabled) {
    painter->setBrush(m_backgroundColor);
  } else {
    painter->setBrush(QColor::fromRgb(230, 230, 230, 100));
  }
  painter->drawPath(m_polyhedronShape);

  painter->drawLines(m_tileLines.data(), m_tileLines.size());

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

  std::unordered_set<VizPoint *> selectedPoints =
      coordinateSystem()->projection()->selectionManager()->selectedPoints();
  if (!selectedPoints.empty()) {
    m_mouseEventForwarding = std::all_of(std::begin(selectedPoints), std::end(selectedPoints), [this](VizPoint *v) {
      return v->polyhedron() == this;
    });
    if (m_mouseEventForwarding) {
      VizPoint *vp = *selectedPoints.begin();
      vp->mousePressEvent(event);
      return;
    }
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

  std::unordered_set<VizPoint *> selectedPoints =
      coordinateSystem()->projection()->selectionManager()->selectedPoints();
  if (m_mouseEventForwarding && !selectedPoints.empty()) {
    VizPoint *vp = *selectedPoints.begin();
    vp->mouseMoveEvent(event);
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

  std::unordered_set<VizPoint *> selectedPoints =
      coordinateSystem()->projection()->selectionManager()->selectedPoints();
  if (m_mouseEventForwarding && !selectedPoints.empty()) {
    m_mouseEventForwarding = false;
    VizPoint *vp = *selectedPoints.begin();
    vp->mouseReleaseEvent(event);
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
  return QPointF(x * pointDistance,
                 -y * pointDistance);
}

void VizPolyhedron::reparentPoint(VizPoint *point) {
  if (point->polyhedron() == this)
    return;

  if (point->polyhedron()) {
    VizPolyhedron *other = point->polyhedron();
    PointMap::iterator it, eit;
    std::tie(it, eit) = other->m_pts.equal_range(point->originalCoordinates());
    PointMap::iterator found = std::find_if(it, eit, [point](auto p) { return p.second == point; });
    if (found != eit) {
      other->m_pts.erase(found);
    }
  }
  point->reparent(this);
  m_pts.emplace(point->originalCoordinates(), point);
}

void VizPolyhedron::recomputeMinMax() {
  int horizontalMin = INT_MAX, horizontalMax = INT_MIN,
      verticalMin = INT_MAX, verticalMax = INT_MIN;
  for (auto p : m_pts) {
    VizPoint *vp = p.second;
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
  for (auto p : m_pts) {
    VizPoint *vp = p.second;
    int xCoord, yCoord;
    std::tie(xCoord, yCoord) = vp->scatteredCoordinates();
    double ratioCoord = static_cast<double>(xCoord - m_localHorizontalMin) /
        static_cast<double>(m_localHorizontalMax - m_localHorizontalMin);
    double ratio = ratioCoord * (extra / pointDistance);
    vp->setPos(mapToCoordinates(xCoord + ratio, yCoord == VizPoint::NO_COORD ? 0 : yCoord));
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
  for (auto p : m_pts) {
    VizPoint *vp = p.second;
    int xCoord, yCoord;
    std::tie(xCoord, yCoord) = vp->scatteredCoordinates();
    double ratioCoord = static_cast<double>(m_localHorizontalMax - xCoord) /
        static_cast<double>(m_localHorizontalMax - m_localHorizontalMin);
    double ratio = ratioCoord * (extra / pointDistance);
    vp->setPos(mapToCoordinates(xCoord + ratio, yCoord == VizPoint::NO_COORD ? 0 : yCoord));
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
  for (auto p : m_pts) {
    VizPoint *vp = p.second;
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
  for (auto p : m_pts) {
    VizPoint *vp = p.second;
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

void VizPolyhedron::prepareRotateAngle(double angle) {
  prepareGeometryChange();
  QTransform transform;
  transform.translate(m_rotationCenter.x(), m_rotationCenter.y());
  transform.rotate(angle);
  transform.translate(-m_rotationCenter.x(), -m_rotationCenter.y());
  m_polyhedronShape = transform.map(m_originalPolyhedronShape);
  for (auto p : m_pts) {
    VizPoint *vp = p.second;
    QPointF position = transform.map(mapToCoordinates(pointScatteredCoordsReal(vp)));
    vp->setPos(position);
  }
}

double VizPolyhedron::prepareRotate(QPointF displacement) {
  QLineF handleLine(m_rotationCenter, m_pressPos);
  QLineF currentLine(m_rotationCenter, m_pressPos + displacement);
  double angle = -handleLine.angleTo(currentLine);

  prepareRotateAngle(angle);
  return -angle;
}

void VizPolyhedron::resetRotate() {
  m_polyhedronShape = m_originalPolyhedronShape;
  resetPointPositions();
}

void VizPolyhedron::handleAboutToMove(const VizHandle *const handle) {
  m_coordinateSystem->projection()->selectionManager()->clearSelection();

  VizManipulationManager *vmm = m_coordinateSystem->projection()->manipulationManager();

  m_originalPolyhedronShape = m_polyhedronShape;
  m_pressPos = handle->rect().center();
  m_rotationCenter = boundingRect().center();
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
    if (handle->isModifierPressed())
      vmm->polyhedronAboutToRotate(this, VizManipulationManager::C_TOP | VizManipulationManager::C_RIGHT);
    else
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
    if (handle->isModifierPressed()) {
      vmm->polyhedronRotating(displacement);
      break;
    }

//    // Do not allow simultaneous horizontal and vertical skew (no support in clay)
//    if (fabs(displacement.x()) < 5 || fabs(displacement.y()) > fabs(displacement.x()))
//      displacement.rx() = 0;
//    else
//      displacement.ry() = 0;
    vmm->polyhedronSkewing(displacement);
    break;
  default:
    break;
  }
}

void VizPolyhedron::handleHasMoved(const VizHandle *const handle, QPointF displacement) {
  prepareGeometryChange();
  VizManipulationManager *vmm = m_coordinateSystem->projection()->manipulationManager();
  m_rotationCenter = QPointF();
  m_pressPos = QPointF();
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
    if (handle->isModifierPressed())
      vmm->polyhedronHasRotated(this);
    else
      vmm->polyhedronHasSkewed(this);
    break;
  default:
    CLINT_UNREACHABLE;
  }
  updateHandlePositions();
}

void VizPolyhedron::disconnectAll() {
  if (m_occurrence) {
    disconnect(m_occurrence, &ClintStmtOccurrence::pointsChanged, this, &VizPolyhedron::occurrenceChanged);
    disconnect(m_occurrence, &ClintStmtOccurrence::destroyed, this, &VizPolyhedron::occurrenceDeleted);
  }

  for (VizHandle *vh : m_handles) {
    disconnect(vh, &VizHandle::moving, this, &VizPolyhedron::handleMoving);
    disconnect(vh, &VizHandle::aboutToMove, this, &VizPolyhedron::handleAboutToMove);
    disconnect(vh, &VizHandle::hasMoved, this, &VizPolyhedron::handleHasMoved);
  }
}

void VizPolyhedron::setOccurrenceImmediate(ClintStmtOccurrence *occurrence) {
  disconnectAll();
  m_occurrence = occurrence;
  if (occurrence) {
    m_backgroundColor = m_coordinateSystem->projection()->vizProperties()->color(occurrence->canonicalOriginalBetaVector());
    connect(occurrence, &ClintStmtOccurrence::pointsChanged, this, &VizPolyhedron::occurrenceChanged);
    connect(occurrence, &ClintStmtOccurrence::destroyed, this, &VizPolyhedron::occurrenceDeleted);

    reprojectPoints();
    recomputeMinMax();
    resetPointPositions();
    recomputeShape();
    recreateTileLines();
    coordinateSystem()->polyhedronUpdated(this);
    updateHandlePositions();
  }
}

void VizPolyhedron::occurrenceDeleted() {
  m_occurrence = nullptr;
}

void VizPolyhedron::debugPrintPoints() {
#ifndef NDEBUG
  for (auto elem : m_pts) {
    std::cerr << "(";
    if (elem.first.size() != 0) {
      std::copy(std::begin(elem.first), --std::end(elem.first), std::ostream_iterator<int>(std::cerr, ", "));
      std::cerr << elem.first.back();
    }
    std::pair<int, int> scattered = elem.second->scatteredCoordinates();
    std::cerr << ") -> (" << scattered.first << ", " << scattered.second << ")" << std::endl;
  }
#endif
}

VizHandle *VizPolyhedron::handle(VizHandle::Kind kind) const {
  for (VizHandle *h : m_handles) {
    if (h->kind() == kind) {
      return h;
    }
  }
  return nullptr;
}

VizHandle::Kind VizPolyhedron::maxDisplacementHandleKind() {
  const VizPolyhedron *animTarget = coordinateSystem()->animationTarget(this);
  CLINT_ASSERT(animTarget != nullptr, "Trying to find maxDisplacementHandle with no animation set up");

  double maximumDistance = -1;
  VizHandle::Kind h;
  for (VizHandle::Kind k : VizHandle::Corners) {
    VizHandle *targetHandle = animTarget->handle(k);
    VizHandle *currentHandle = handle(k);
    double distance = QLineF(targetHandle->center(), currentHandle->center()).length();
    if (distance > maximumDistance) {
      maximumDistance = distance;
      h = k;
    }
  }
  return h;
}

void VizPolyhedron::reparent(VizCoordinateSystem *vcs) {
  if (vcs == m_coordinateSystem)
    return;
  m_coordinateSystem = vcs;
  QPointF scenePosition = scenePos();
  setParentItem(vcs);
  vcs->setIgnorePolyhedraPositionUpdates();
  coordinateSystem()->projection()->updateSceneLayout();
  setPos(vcs->mapFromScene(scenePosition));

  const std::vector<VizPolyhedron *> polyhedra = coordinateSystem()->polyhedra();
  auto it = std::find(std::begin(polyhedra), std::end(polyhedra), this);
  size_t index = std::distance(std::begin(polyhedra), it);
  double offset = coordinateSystem()->projection()->vizProperties()->polyhedronOffset() * index;

  enlargeCoordinateSystem();
  QPropertyAnimation *anim = new QPropertyAnimation(this, "pos", this);
  anim->setDuration(coordinateSystem()->projection()->vizProperties()->animationDuration());
  anim->setEndValue(QPointF(offset, -offset));
  anim->start();
  m_transitionAnimation = anim;
  connect(m_transitionAnimation, &QAbstractAnimation::finished, this, &VizPolyhedron::betaTransitionAnimationFinished);
}

void VizPolyhedron::betaTransitionAnimationFinished() {
  m_coordinateSystem->setIgnorePolyhedraPositionUpdates(false);
  disconnect(m_transitionAnimation, &QAbstractAnimation::finished,
             this, &VizPolyhedron::betaTransitionAnimationFinished);
}
