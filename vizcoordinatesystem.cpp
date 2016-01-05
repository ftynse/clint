#include "vizcoordinatesystem.h"
#include "vizpolyhedron.h"
#include "vizprojection.h"
#include "vizdeparrow.h"
#include "vizpoint.h"
#include "clintstmtoccurrence.h"
#include "clintdependence.h"
#include "oslutils.h"
#include "enumerator.h"

#include <QtCore>
#include <QtGui>
#include <QtWidgets>

VizCoordinateSystem::VizCoordinateSystem(VizProjection *projection, size_t horizontalDimensionIdx, size_t verticalDimensionIdx, QGraphicsItem *parent) :
  QGraphicsObject(parent), m_projection(projection), m_horizontalDimensionIdx(horizontalDimensionIdx), m_verticalDimensionIdx(verticalDimensionIdx) {

  if (m_horizontalDimensionIdx == VizProperties::NO_DIMENSION)
    m_horizontalAxisState = AxisState::Invisible;
  if (m_verticalDimensionIdx == VizProperties::NO_DIMENSION)
    m_verticalAxisState = AxisState::Invisible;

  m_font = qApp->font();  // Setting up default font for the view.  Can be adjusted afterwards.
}

void VizCoordinateSystem::setVerticalDimensionIdx(size_t verticalDimensionIdx) {
  m_verticalDimensionIdx = verticalDimensionIdx;
  if (m_verticalDimensionIdx == VizProperties::NO_DIMENSION)
    m_verticalAxisState = AxisState::Invisible;
  else
    m_verticalAxisState = AxisState::Visible;
  for (VizPolyhedron *vp : m_polyhedra) {
    vp->occurrenceChanged();
  }
}

void VizCoordinateSystem::setHorizontalDimensionIdx(size_t horizontalDimensionIdx) {
  m_horizontalDimensionIdx = horizontalDimensionIdx;
  if (m_horizontalDimensionIdx == VizProperties::NO_DIMENSION)
    m_horizontalAxisState = AxisState::Invisible;
  else
    m_horizontalAxisState = AxisState::Visible;
  for (VizPolyhedron *vp : m_polyhedra) {
    vp->occurrenceChanged();
  }
}

void VizCoordinateSystem::addAxisLabels(ClintStmtOccurrence *occurrence) {
  if (m_horizontalAxisState == AxisState::Visible ||
      m_horizontalAxisState == AxisState::WillDisappear) {
    const char *horizontalName = occurrence->statement()->dimensionName(m_horizontalDimensionIdx).c_str();
    if (m_horizontalName.size() == 0) {
      m_horizontalName = QString(horizontalName);
    } else {
      m_horizontalName += QString(",%1").arg(horizontalName);
    }
  }
  if (m_verticalAxisState == AxisState::Visible ||
      m_verticalAxisState == AxisState::WillDisappear) {
    const char *verticalName = occurrence->statement()->dimensionName(m_verticalDimensionIdx).c_str();
    if (m_verticalName.size() == 0) {
      m_verticalName = QString(verticalName);
    } else {
      m_verticalName += QString(",%1").arg(verticalName);
    }
  }
}

void VizCoordinateSystem::regenerateAxisLabels() {
  m_horizontalName.clear();
  m_verticalName.clear();
  for (VizPolyhedron *vp : m_polyhedra) {
    addAxisLabels(vp->occurrence());
  }
}

bool VizCoordinateSystem::projectStatementOccurrence(ClintStmtOccurrence *occurrence) {
  // Check if the axes are displayed properly.
  CLINT_ASSERT((occurrence->dimensionality() <= m_horizontalDimensionIdx) ^ (m_horizontalAxisState == AxisState::Visible),
               "Projecting statement on the axis-less dimension");
  CLINT_ASSERT((occurrence->dimensionality() <= m_verticalDimensionIdx) ^ (m_verticalAxisState == AxisState::Visible),
               "Projecting statement on the axis-less dimension");

  // Add iterator variable names to the list.
  addAxisLabels(occurrence);

  VizPolyhedron *vp = new VizPolyhedron(occurrence, this);
  if (!vp->hasPoints()) {
    delete vp;
    return false;
  }

  m_polyhedra.push_back(vp);
  polyhedronUpdated(vp);

  int occurrenceHorizontalMin = occurrence->minimumValue(m_horizontalDimensionIdx);
  int occurrenceHorizontalMax = occurrence->maximumValue(m_horizontalDimensionIdx);
  int occurrenceVerticalMin   = occurrence->minimumValue(m_verticalDimensionIdx);
  int occurrenceVerticalMax   = occurrence->maximumValue(m_verticalDimensionIdx);
  setMinMax(std::min(occurrenceHorizontalMin, m_horizontalMin),
            std::max(occurrenceHorizontalMax, m_horizontalMax),
            std::min(occurrenceVerticalMin, m_verticalMin),
            std::max(occurrenceVerticalMax, m_verticalMax));

  const VizProperties *props = projection()->vizProperties();
  if (props->filledPolygons())
    vp->setColor(projection()->vizProperties()->color(occurrence->betaVector()));
  else
    vp->setColor(QColor(Qt::transparent));

  vp->updateInternalDependences();

  return true;
}

void VizCoordinateSystem::deleteInnerDependences() {
  for (VizDepArrow *vda : m_depArrows) {
    vda->setParentItem(nullptr);
    vda->setVisible(false);
//    delete vda;
  }
  m_depArrows.clear();
}

void VizCoordinateSystem::updateInnerDependences() {
  // TODO: update old arrows instead of creating new? (split/fuse may force deletion)
  deleteInnerDependences();

  for (VizPolyhedron *vp1 : m_polyhedra) {
    for (VizPolyhedron *vp2 : m_polyhedra) {
      if (vp1 == vp2)
        continue;

      std::unordered_set<ClintDependence *> dependences =
          vp1->scop()->dependencesBetween(vp1->occurrence(), vp2->occurrence());
      if (dependences.empty())
        continue;

      for (ClintDependence *dep : dependences) {
        std::vector<std::vector<int>> lines =
            dep->projectOn(m_horizontalDimensionIdx, m_verticalDimensionIdx);
        setInnerDependencesBetween(vp1, vp2, std::move(lines), dep->isViolated());
      }
    }
  }

  update();
}

void VizCoordinateSystem::setInnerDependencesBetween(VizPolyhedron *vp1, VizPolyhedron *vp2,
                                                     std::vector<std::vector<int>> &&lines, bool violated) {
  vizDependenceArrowsCreate(vp1, vp2, std::forward<std::vector<std::vector<int>>>(lines), violated, this, m_depArrows);
}

void VizCoordinateSystem::updateInternalDependences() {
  for (VizPolyhedron *vp : m_polyhedra) {
    vp->updateInternalDependences();
  }
}


std::vector<int> VizCoordinateSystem::betaPrefix() const {
//  // FIXME: this works only for the first two dimensions, betas should be set up in construction
//  bool enabled = ((m_horizontalAxisVisible && m_horizontalDimensionIdx == 0) || !m_horizontalAxisVisible) &&
//                 ((m_verticalAxisVisible && m_verticalDimensionIdx == 1) || !m_verticalAxisVisible);
//  CLINT_ASSERT(enabled, "Cannot find a correct beta-prefix, for a projection not on <0,1>");

//  std::pair<size_t, size_t> indices = m_projection->csIndices(this);
//  std::vector<int> beta;
//  if (m_horizontalAxisVisible)
//    beta.push_back(indices.first);
//  else
//    return beta;
//  if (m_verticalAxisVisible)
//    beta.push_back(indices.second);
//  return beta;
  CLINT_ASSERT(!isEmpty(), "Can't find a beta-prefix of an empty coordinate system");
  VizPolyhedron *vp = m_polyhedra.front();
  std::vector<int> beta = vp->occurrence()->betaVector();
  if (vp->occurrence()->dimensionality() >= m_horizontalDimensionIdx) {
    beta.erase(std::end(beta) - 1);
  }
  return beta;
}

void VizCoordinateSystem::updatePolyhedraPositions() {
  for (size_t i = 0, iend = m_polyhedra.size(); i < iend; i++) {
    VizPolyhedron *vph = m_polyhedra.at(i);
    setAnyPolyhedronPosition(vph, vph->localHorizontalMin(), vph->localVerticalMin(), i);
    vph->setZValue(m_polyhedra.size() + 1 - i);
  }
}

void VizCoordinateSystem::updateAllPositions() {
  // This does not update arrows since they are attached to the polyhedra by signal/slot
  updatePolyhedraPositions();
  for (VizPolyhedron *vp : m_polyhedra) {
    vp->updateShape();
  }
}

void VizCoordinateSystem::setAnyPolyhedronPosition(VizPolyhedron *polyhedron, int horizontal,
                                                   int vertical, ssize_t idx,
                                                   bool ignoreHorizontal, bool ignoreVertical) {
  double offset = m_projection->vizProperties()->polyhedronOffset() * idx;
  QPointF position = polyhedron->pos();
  double hpos = ignoreHorizontal ?
        position.x() :
        offset;
  double vpos = ignoreVertical ?
        position.y() :
        -offset;
  polyhedron->setPos(hpos, vpos);
}

void VizCoordinateSystem::setPolyhedronCoordinates(VizPolyhedron *polyhedron, int horizontal,
                                                   int vertical, bool ignoreHorizontal,
                                                   bool ignoreVertical) {
  auto it = std::find(std::begin(m_polyhedra), std::end(m_polyhedra), polyhedron);
  CLINT_ASSERT(it != std::end(m_polyhedra),
               "Polyhedron updated does not belong to the coordinate system");

  ssize_t idx = std::distance(std::begin(m_polyhedra), it);
  setAnyPolyhedronPosition(polyhedron, horizontal, vertical, idx, ignoreHorizontal, ignoreVertical);
  polyhedron->setZValue(m_polyhedra.size() + 1 - idx);
}

void VizCoordinateSystem::resetPolyhedronPos(VizPolyhedron *polyhedron) {
  setPolyhedronCoordinates(polyhedron,
                           polyhedron->localHorizontalMin(),
                           polyhedron->localVerticalMin());
}

void VizCoordinateSystem::createPolyhedronShadow(VizPolyhedron *polyhedron) {
  auto it = std::find(std::begin(m_polyhedra), std::end(m_polyhedra), polyhedron);
  CLINT_ASSERT(it != std::end(m_polyhedra),
               "Polyhedron updated does not belong to the coordinate system");
  ssize_t index = std::distance(std::begin(m_polyhedra), it);

  VizPolyhedron *shadow = m_polyhedra[index]->createShadow();
  setAnyPolyhedronPosition(shadow, shadow->localHorizontalMin(), shadow->localVerticalMin(), index);
  shadow->setZValue(index);
  m_polyhedronShadows.emplace(index, shadow);
}

void VizCoordinateSystem::createPolyhedronAnimationTarget(VizPolyhedron *polyhedron) {
  auto it = std::find(std::begin(m_polyhedra), std::end(m_polyhedra), polyhedron);
  CLINT_ASSERT(it != std::end(m_polyhedra),
               "Polyhedron updated does not belong to the coordinate system");
  ssize_t index = std::distance(std::begin(m_polyhedra), it);
  VizPolyhedron *shadow = polyhedron->createShadow(false);
  setAnyPolyhedronPosition(shadow, shadow->localHorizontalMin(), shadow->localVerticalMin(), index);
  m_polyhedronAnimationTargets.emplace(polyhedron, shadow);
}

void VizCoordinateSystem::clearPolyhedronShadows() {
  for (auto p : m_polyhedronShadows) {
    VizPolyhedron *shadow = p.second;
    shadow->setVisible(false);
    shadow->setParentItem(nullptr);
    shadow->setParent(nullptr);
    delete shadow;
  }
  m_polyhedronShadows.clear();
}

void VizCoordinateSystem::clearPolyhedronAnimationTargets() {
  for (auto p : m_polyhedronAnimationTargets) {
    VizPolyhedron *shadow = p.second;
    shadow->setVisible(false);
    shadow->setParentItem(nullptr);
    shadow->setParent(nullptr);
    delete shadow;
  }
  m_polyhedronAnimationTargets.clear();
}

void VizCoordinateSystem::finalizeOccurrenceChange() {
  for (auto p : m_polyhedronShadows) {
    size_t index = p.first;
    m_polyhedra[index]->finalizeOccurrenceChange();
  }
}

void VizCoordinateSystem::reparentPolyhedron(VizPolyhedron *polyhedron) {
  VizCoordinateSystem *oldCS = polyhedron->coordinateSystem();
  if (oldCS == this)
    return;
  oldCS->removePolyhedron(polyhedron);
  m_polyhedra.push_back(polyhedron);
  addAxisLabels(polyhedron->occurrence());
  polyhedron->reparent(this);
}

void VizCoordinateSystem::insertPolyhedronAfter(VizPolyhedron *inserted, VizPolyhedron *after) {
  auto it = std::find(std::begin(m_polyhedra), std::end(m_polyhedra), after);
  if (it == std::end(m_polyhedra))
    m_polyhedra.push_back(inserted);
  else
    m_polyhedra.insert(++it, inserted);
  if (inserted->occurrence())
    addAxisLabels(inserted->occurrence());
  inserted->reparent(this);

  updatePolyhedraPositions();
}

void VizCoordinateSystem::removePolyhedron(VizPolyhedron *polyhedron) {
  auto it = std::find(std::begin(m_polyhedra), std::end(m_polyhedra), polyhedron);
  CLINT_ASSERT(it != std::end(m_polyhedra),
               "Trying to remove the polyhedron that is not present in the CS.");
  m_polyhedra.erase(it);
  polyhedron->setParentItem(nullptr);
  regenerateAxisLabels();
}

void VizCoordinateSystem::polyhedronUpdated(VizPolyhedron *polyhedron) {
  const double pointDistance = m_projection->vizProperties()->pointDistance();
  auto it = std::find(std::begin(m_polyhedra), std::end(m_polyhedra), polyhedron);
  if (it == std::end(m_polyhedra))
    return;
  double offset = m_projection->vizProperties()->polyhedronOffset() * (it - std::begin(m_polyhedra));

  // check (assert) if the actual position corresponds to the computed one;
  QPointF expected;
  expected.rx() = offset + (polyhedron->localHorizontalMin() - m_horizontalMin + 1) * pointDistance;
  expected.ry() = -(offset + (polyhedron->localVerticalMin() - m_verticalMin + 1) * pointDistance);

  // update it to fit in the grid
  // TODO: animation
//  polyhedron->setPos(expected);
}

void VizCoordinateSystem::setMinMax(int horizontalMinimum, int horizontalMaximum,
                                    int verticalMinimum, int verticalMaximum) {
  m_horizontalMin = horizontalMinimum;
  m_horizontalMax = horizontalMaximum;
  m_verticalMin   = verticalMinimum;
  m_verticalMax   = verticalMaximum;
  updatePolyhedraPositions();
}

void VizCoordinateSystem::setHorizontalMinMax(int horizontalMinimum, int horizontalMaximum) {
  m_horizontalMin = horizontalMinimum;
  m_horizontalMax = horizontalMaximum;
  updatePolyhedraPositions();
}

void VizCoordinateSystem::setVerticalMinMax(int verticalMinimum, int verticalMaximum) {
  m_verticalMin   = verticalMinimum;
  m_verticalMax   = verticalMaximum;
  updatePolyhedraPositions();
}

QPolygonF VizCoordinateSystem::leftArrow(int length, const double pointRadius)
{
  QPolygonF triangle;
  triangle.append(QPointF(length, 0));
  triangle.append(QPointF(length - 2 * pointRadius, pointRadius));
  triangle.append(QPointF(length - 2 * pointRadius, -pointRadius));
  triangle.append(triangle.front());

  return triangle;
}

QPolygonF VizCoordinateSystem::topArrow(int length, const double pointRadius)
{
  QPolygonF triangle;
  triangle.append(QPointF(0, -length));
  triangle.append(QPointF(pointRadius, -length + 2 * pointRadius));
  triangle.append(QPointF(-pointRadius, -length + 2 * pointRadius));
  triangle.append(triangle.front());

  return triangle;
}

void VizCoordinateSystem::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) {
  Q_UNUSED(option);
  Q_UNUSED(widget);

  const double pointRadius = m_projection->vizProperties()->pointRadius();
  const double pointDistance = m_projection->vizProperties()->pointDistance();

  QFontMetrics fm = painter->fontMetrics();
  painter->setBrush(Qt::black);
  painter->setFont(m_font);
  painter->setRenderHint(QPainter::Antialiasing);
  painter->setRenderHint(QPainter::TextAntialiasing);

  QTransform transform = QTransform::fromTranslate(
         (m_horizontalMin - 1) * pointDistance,
        -(m_verticalMin - 1) * pointDistance);
  painter->setTransform(transform, true);

  if (m_horizontalAxisState == AxisState::Visible ||
      m_horizontalAxisState == AxisState::WillDisappear) {
    painter->save();
    if (m_horizontalAxisState == AxisState::WillDisappear) {
      painter->setPen(QPen(QBrush(Qt::gray), 1.0, Qt::DashLine));
      painter->setBrush(QBrush(Qt::gray));
    }
    // Draw axis.
    int length = horizontalAxisLength();
    painter->drawLine(0, 0, length, 0);
    // Draw arrow.
    QPolygonF triangle = leftArrow(length, pointRadius);
    painter->drawConvexPolygon(triangle);

    // Draw tics.
    for (int i = m_horizontalMin; i <= m_horizontalMax; i++) {
      int pos = (1 + (i - m_horizontalMin)) * pointDistance;
      painter->drawLine(pos, -pointRadius / 2., pos, pointRadius / 2.);
      QString ticText = QString("%1").arg(i);
      painter->drawText(pos - pointDistance / 2.,
                        ticMargin(),
                        pointDistance,
                        fm.lineSpacing(),
                        Qt::AlignHCenter | Qt::AlignBottom | Qt::TextDontClip, ticText);
    }

    // Draw label.
    int pos = (3 + m_horizontalMax - m_horizontalMin) * pointDistance;
    painter->drawText(pos - pointDistance / 2.,
                      ticMargin(),
                      fm.width(m_horizontalName),
                      fm.lineSpacing(),
                      Qt::AlignHCenter | Qt::AlignBottom | Qt::TextDontClip, m_horizontalName);
    painter->restore();
  } else if (m_horizontalAxisState == AxisState::WillAppear) {
    painter->save();
    painter->setPen(Qt::gray);
    painter->setBrush(Qt::gray);
    painter->drawLine(0, 0, 2 * pointDistance, 0);
    painter->drawConvexPolygon(leftArrow(2 * pointDistance, pointRadius));
    painter->restore();
  }

  if (m_verticalAxisState == AxisState::Visible ||
      m_verticalAxisState == AxisState::WillDisappear) {
    painter->save();
    if (m_verticalAxisState == AxisState::WillDisappear) {
      painter->setPen(QPen(QBrush(Qt::gray), 1.0, Qt::DashLine));
      painter->setBrush(QBrush(Qt::gray));
    }
    // Draw axis.
    int length = verticalAxisLength();
    painter->drawLine(0, 0, 0, -length);
    // Draw arrow.
    QPolygonF triangle = topArrow(length, pointRadius);
    painter->drawConvexPolygon(triangle);

    // Draw tics.
    for (int i = m_verticalMin; i <= m_verticalMax; i++) {
      int pos = -(1 + (i - m_verticalMin)) * pointDistance;
      painter->drawLine(-pointRadius / 2., pos, pointRadius / 2., pos);
      QString ticText = QString("%1").arg(i);
      int textWidth = fm.width(ticText);
      painter->drawText(-textWidth - ticMargin(),
                        pos - pointDistance / 2.,
                        textWidth,
                        pointDistance,
                        Qt::AlignVCenter | Qt::AlignRight | Qt::TextDontClip, ticText);
    }

    // Draw label.
    int pos = -(3 + m_verticalMax - m_verticalMin) * pointDistance;
    int textWidth = fm.width(m_verticalName);
    painter->drawText(-textWidth - ticMargin(),
                      pos - pointDistance / 2.,
                      textWidth,
                      pointDistance,
                      Qt::AlignVCenter | Qt::AlignRight | Qt::TextDontClip, m_verticalName);
    painter->restore();
  } else if (m_verticalAxisState == AxisState::WillAppear) {
    painter->save();
    painter->setPen(Qt::gray);
    painter->setBrush(Qt::gray);
    painter->drawLine(0, 0, 0, -2 * pointDistance);
    painter->drawConvexPolygon(topArrow(2*pointDistance, pointRadius));
    painter->restore();
  }

  if (nextCsIsDependent) {
    QPointF position(horizontalAxisLength() / 2, -verticalAxisLength());
    painter->save();
    if (nextCsIsViolated) {
      painter->setBrush(Qt::red);
    }
    painter->drawEllipse(position, 4, 4);
    painter->restore();
  }

  if (nextPileIsDependent) {
    QPointF position(horizontalAxisLength(), -verticalAxisLength() / 2);
    painter->save();
    if (nextPileIsViolated) {
      painter->setBrush(Qt::red);
    }
    painter->drawEllipse(position, 4, 4);
    painter->restore();
  }

  painter->resetTransform();
}

QRectF VizCoordinateSystem::boundingRect() const {
  int width  = horizontalAxisLength();
  int height = verticalAxisLength();
  int left   = 0;
  int top    = -height;

  QFontMetrics fm(m_font);
  int ticWidth = 0;
  for (int i = m_verticalMin; i <= m_verticalMax; i++) {
    ticWidth = std::max(ticWidth, fm.width(QString("%1").arg(i)));
  }
  ticWidth += ticMargin();

  width += ticWidth;
  left -= ticWidth;
  height += ticMargin() + fm.height();
  return QRectF(left, top, width, height) | childrenBoundingRect();
}

QRectF VizCoordinateSystem::coordinateSystemRect() const {
  int width  = horizontalAxisLength();
  int height = verticalAxisLength();
  return QRectF(0, -height, width, height);
}

int VizCoordinateSystem::horizontalAxisLength() const {
  return (m_horizontalMax - m_horizontalMin + 3) * m_projection->vizProperties()->pointDistance();
}

int VizCoordinateSystem::verticalAxisLength() const {
  return (m_verticalMax - m_verticalMin + 3) * m_projection->vizProperties()->pointDistance();
}

int VizCoordinateSystem::ticMargin() const {
  return 2 * m_projection->vizProperties()->pointRadius();
}

int VizCoordinateSystem::dependentWith(VizCoordinateSystem *vcs) {
  bool dependent = false;
  bool violated = false;
  for (VizPolyhedron *vp1 : m_polyhedra) {
    for (VizPolyhedron *vp2 : vcs->m_polyhedra) {
      ClintScop *scop = vp1->scop();
      std::unordered_set<ClintDependence *> deps =
          scop->dependencesBetween(vp1->occurrence(), vp2->occurrence());
      dependent = dependent || !deps.empty();
      for (ClintDependence *dep : deps) {
        violated = violated || dep->isViolated();
      }
      if (violated)
        break;
    }
  }
  if (violated)
    return 2;
  if (dependent)
    return 1;
  return 0;
}

VizPolyhedron *VizCoordinateSystem::polyhedron(const std::vector<int> &beta) const {
  VizPolyhedron *result = nullptr;
  for (VizPolyhedron *vp : m_polyhedra) {
    if (BetaUtility::isPrefixOrEqual(beta, vp->occurrence()->betaVector()))  {
      CLINT_ASSERT(result == nullptr, "Expecting only one polyhedron with given beta-prefix"); // implement a different function if needed
      result = vp;
    }
  }
  return result;
}

VizPolyhedron *VizCoordinateSystem::shadow(VizPolyhedron *original) const {
  auto it = std::find(std::begin(m_polyhedra), std::end(m_polyhedra), original);
  if (it == std::end(m_polyhedra))
    return nullptr;
  int index = std::distance(std::begin(m_polyhedra), it);
  if (m_polyhedronShadows.count(index) != 0) {
    return m_polyhedronShadows.at(index);
  }
  return nullptr;
}

VizPolyhedron *VizCoordinateSystem::animationTarget(VizPolyhedron *original) const {
  if (m_polyhedronAnimationTargets.count(original) != 0) {
    return m_polyhedronAnimationTargets.at(original);
  }
  return nullptr;
}
