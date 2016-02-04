#include "clintstmtoccurrence.h"
#include "clintscop.h"
#include "oslutils.h"
#include "projectionview.h"
#include "vizcoordinatesystem.h"
#include "vizprojection.h"

#include <QtWidgets>
#include <QtCore>

#include <algorithm>
#include <map>
#include <vector>

VizProjection::VizProjection(int horizontalDimensionIdx, int verticalDimensionIdx, ClintScop *vscop, QObject *parent) :
  QObject(parent), m_scop(vscop),
  m_horizontalDimensionIdx(horizontalDimensionIdx),
  m_verticalDimensionIdx(verticalDimensionIdx) {

  m_scene = new QGraphicsScene(this);
  m_view = new ProjectionView(m_scene);
  connect(m_view, &ProjectionView::doubleclicked, this, &VizProjection::selectProjection);

  m_view->setDragMode(QGraphicsView::RubberBandDrag);
  m_view->setRubberBandSelectionMode(Qt::ContainsItemShape);

  m_vizProperties = new VizProperties(this);
  connect(m_vizProperties, &VizProperties::vizPropertyChanged,
          this, &VizProjection::updateProjection);

  m_selectionManager = new VizSelectionManager(this);
  m_manipulationManager = new VizManipulationManager(this);

  projectScop();
}

void VizProjection::updateProjection() {
  updateSceneLayout();
  for (auto pile : m_coordinateSystems) {
    for (VizCoordinateSystem *vcs : pile) {
      vcs->updateAllPositions();
    }
  }
  m_view->viewport()->update();
}

void VizProjection::finalizeOccurrenceChange() {
  for (auto pile : m_coordinateSystems) {
    for (VizCoordinateSystem *vcs : pile) {
      vcs->finalizeOccurrenceChange();
    }
  }
}

void VizProjection::correctIsCs(IsCsResult &result, VizPolyhedron *polyhedron) {
  VizCoordinateSystem *referenceCS;
  if (result.m_pile >= m_coordinateSystems.size()) {
    referenceCS = m_coordinateSystems.back().front();
  } else {
    referenceCS = m_coordinateSystems.at(result.m_pile).front();
  }
  std::vector<int> referenceBeta = (*referenceCS->polyhedra().begin())->occurrence()->betaVector();
  std::vector<int> beta = polyhedron->occurrence()->betaVector();

  if (BetaUtility::partialMatch(referenceBeta, beta) < m_horizontalDimensionIdx) {
    result.m_action = IsCsAction::InsertPile;
    if (BetaUtility::follows(referenceBeta, beta)) {
      // look at leftmost (go right)
      bool retargeted = false;
      for (size_t p = result.m_pile + 1; p < m_coordinateSystems.size(); ++p) {
        referenceCS = m_coordinateSystems[p].front();
        referenceBeta = (*referenceCS->polyhedra().begin())->occurrence()->betaVector();
        if (BetaUtility::partialMatch(beta, referenceBeta) >= m_horizontalDimensionIdx) {
          result.m_pile = p;
          retargeted = true;
          break;
        }
      }
      CLINT_ASSERT(retargeted, "Couldn't correct pile insertion index");
    } else if (BetaUtility::follows(beta, referenceBeta)) {
      // look at rightmost (go left)
      bool retargeted = false;
      CLINT_ASSERT(result.m_pile != 0, "Couldn't correct pile insertion index");
      for (size_t s = result.m_pile; s > 0; --s) {
        size_t p = s - 1;
        referenceCS = m_coordinateSystems[p].front();
        referenceBeta = (*referenceCS->polyhedra().begin())->occurrence()->betaVector();
        if (BetaUtility::partialMatch(referenceBeta, beta) >= m_horizontalDimensionIdx) {
          result.m_pile = p + 1;
          retargeted = true;
          break;
        }
      }
      CLINT_ASSERT(retargeted, "Couldn't correct pile insertion index");
    } else {
      CLINT_UNREACHABLE;
    }
  }

  if (result.m_action == IsCsAction::InsertPile ||
      !polyhedron->coordinateSystem()->isVerticalAxisVisible())
    return;

  // Now assume the polygon is already in the proper tile (and is at the end of it after fusion).
  const std::vector<VizCoordinateSystem *> &pile = m_coordinateSystems[result.m_pile];

  // FIXME: this simulates beta-transformation, maybe use beta-mapper for it.
  // The polygon is first split away until depth, than put last in pile. (technically, we cannot always put it last in pile).
  beta[m_horizontalDimensionIdx] = result.m_pile;
  beta[m_verticalDimensionIdx] = pile.size();
  for (size_t d = m_verticalDimensionIdx + 1; d < beta.size(); ++d) {
    beta[d] = 0;
  }

  if (result.m_coordinateSystem >= pile.size()) {
    referenceCS = pile.back();
  } else {
    referenceCS = pile.at(result.m_coordinateSystem);
  }
  referenceBeta = (*referenceCS->polyhedra().begin())->occurrence()->betaVector();
  if (BetaUtility::partialMatch(referenceBeta, beta) < m_verticalDimensionIdx) {
    result.m_action = IsCsAction::InsertCS;
    if (BetaUtility::follows(referenceBeta, beta)) {
      bool retargeted = false;
      for (size_t c = result.m_coordinateSystem + 1; c < pile.size(); ++c) {
        referenceCS = pile[c];
        referenceBeta = (*referenceCS->polyhedra().begin())->occurrence()->betaVector();
        if (BetaUtility::partialMatch(beta, referenceBeta) >= m_verticalDimensionIdx) {
          result.m_coordinateSystem = c;
          retargeted = true;
          break;
        }
      }
      CLINT_ASSERT(retargeted, "Couldn't correct coordinate system insertion index");
    } else if (BetaUtility::follows(beta, referenceBeta)) {
      bool retargeted = false;
      CLINT_ASSERT(result.m_coordinateSystem != 0 , "Couldn't correct coordinate system insertion index");
      for (size_t s = result.m_coordinateSystem; s > 0; --s) {
        size_t c = s - 1;
        referenceCS = pile[c];
        referenceBeta = (*referenceCS->polyhedra().begin())->occurrence()->betaVector();
        if (BetaUtility::partialMatch(referenceBeta, beta) >= m_verticalDimensionIdx) {
          result.m_coordinateSystem = c + 1;
          retargeted = true;
          break;
        }
      }
      CLINT_ASSERT(retargeted, "Couldn't correct coordinate system insertion index");
    } else {
      CLINT_UNREACHABLE;
    }
  }

  if (result.m_action == IsCsAction::InsertCS)
    return;
}

VizProjection::IsCsResult VizProjection::findCoordinateSystem(QPointF point) {
  bool found = false;
  size_t pileIndex = static_cast<size_t>(-1);
  IsCsResult result;
  for (size_t i = 0, ei = m_coordinateSystems.size(); i < ei; i++) {
    const std::vector<VizCoordinateSystem *> &pile = m_coordinateSystems.at(i);
    VizCoordinateSystem *coordinateSystem = pile.front();
    double csLeft = coordinateSystem->pos().x() + coordinateSystem->coordinateSystemRect().left();
    if (point.x() < csLeft) {
      found = true;
      // hypothetical pile found
      if (i == 0) {
        // add before first pile
        result.m_action = IsCsAction::InsertPile;
        result.m_pile = 0;
        return result;
      } else {
        VizCoordinateSystem *previousCS = m_coordinateSystems.at(i - 1).front();
        QRectF csRect = previousCS->coordinateSystemRect();
        double csRight = previousCS->pos().x() + csRect.right();
        if (point.x() < csRight) {
          // within pile (i - 1)
          pileIndex = i - 1;
        } else {
          // add between piles (i - 1) and (i)
          result.m_action = IsCsAction::InsertPile;
          result.m_pile = i;
          return result;
        }
      }
      break;
    }
  }

  if (!found) {
    VizCoordinateSystem *coordinateSystem = m_coordinateSystems.back().front();
    QRectF csRect = coordinateSystem->coordinateSystemRect();
    double csRight = coordinateSystem->pos().x() + csRect.right();
    if (point.x() < csRight) {
      // within last pile
      pileIndex = m_coordinateSystems.size() - 1;
    } else {
      // add after last pile
      result.m_action = IsCsAction::InsertPile;
      result.m_pile = m_coordinateSystems.size();
      return result;
    }
  }

  CLINT_ASSERT(pileIndex != static_cast<size_t>(-1), "Pile was neither found, nor created");

  result.m_pile = pileIndex;

  std::vector<VizCoordinateSystem *> pile = m_coordinateSystems.at(pileIndex);
  for (size_t i = 0, ei = pile.size(); i < ei; i++) {
    VizCoordinateSystem *coordinateSystem = pile.at(i);
    double csBottom = coordinateSystem->pos().y() + coordinateSystem->coordinateSystemRect().bottom();
    if (point.y() > csBottom) {
      if (i == 0) {
        // add before first cs
        result.m_action = IsCsAction::InsertCS;
        result.m_coordinateSystem = 0;
        return result;
      } else {
        VizCoordinateSystem *previousCS = pile.at(i - 1);
        QRectF csRect = previousCS->coordinateSystemRect();
        double csTop = previousCS->pos().y() + csRect.top();
        if (point.y() > csTop) {
          // within cs (i - 1)
          result.m_action = IsCsAction::Found;
          result.m_vcs = previousCS;
          result.m_coordinateSystem = i - 1;
          return result;
        } else {
          // add between cs (i-1) and (i)
          result.m_action = IsCsAction::InsertCS;
          result.m_coordinateSystem = i;
          return result;
        }
      }
      break;
    }
  }

  VizCoordinateSystem *coordinateSystem = pile.back();
  QRectF csRect = coordinateSystem->coordinateSystemRect();
  double csTop = coordinateSystem->pos().y() + csRect.top();
  if (point.y() > csTop) {
    // within last cs
    result.m_action = IsCsAction::Found;
    result.m_vcs = coordinateSystem;
    result.m_coordinateSystem = m_coordinateSystems.at(pileIndex).size() - 1;
  } else {
    // add after last cs
    result.m_action = IsCsAction::InsertCS;
    result.m_coordinateSystem = pile.size();
  }

  return result;
}

VizProjection::IsCsResult VizProjection::isCoordinateSystem(QPointF point, VizPolyhedron *polyhedron) {
  IsCsResult result = findCoordinateSystem(point);
  correctIsCs(result, polyhedron);
  return result;
}

VizCoordinateSystem *VizProjection::insertPile(IsCsResult csAt, int dimensionality) {
  VizCoordinateSystem *vcs = createCoordinateSystem(dimensionality);
  m_coordinateSystems.insert(std::next(std::begin(m_coordinateSystems), csAt.pileIdx()),
                             std::vector<VizCoordinateSystem *> {vcs});
  updateSceneLayout();
  return vcs;
}

VizCoordinateSystem *VizProjection::insertCs(IsCsResult csAt, int dimensionality) {
  CLINT_ASSERT(csAt.pileIdx() < m_coordinateSystems.size(), "Inserting CS in a non-existent pile");
  VizCoordinateSystem *vcs = createCoordinateSystem(dimensionality);
  std::vector<VizCoordinateSystem *> &pile = m_coordinateSystems.at(csAt.pileIdx());
  pile.insert(std::next(std::begin(pile), csAt.coordinateSystemIdx()), vcs);
  updateSceneLayout();
  return vcs;
}

std::pair<size_t, size_t> VizProjection::csIndices(VizCoordinateSystem *vcs) const {
  size_t pileIdx = static_cast<size_t>(-1);
  size_t csIdx   = static_cast<size_t>(-1);
  for (size_t i = 0; i < m_coordinateSystems.size(); ++i) {
    for (size_t j = 0; j < m_coordinateSystems[i].size(); ++j) {
      if (m_coordinateSystems[i][j] == vcs) {
        pileIdx = i;
        csIdx = j;
        break;
      }
    }
  }
  return std::make_pair(pileIdx, csIdx);
}

VizCoordinateSystem *VizProjection::ensureCoordinateSystem(IsCsResult &csAt, int dimensionality) {
  VizCoordinateSystem *vcs = nullptr;
  switch (csAt.action()) {
  case IsCsAction::Found:
    vcs = csAt.coordinateSystem();
    if (vcs->horizontalDimensionIdx() == VizProperties::NO_DIMENSION) {
      if (dimensionality >= m_horizontalDimensionIdx) {
        // Target projection does not have horizontal dimension, but the polyhedron has and the projection covers it.
        // -> create new pile
        csAt.m_action = IsCsAction::InsertPile;
        return insertPile(csAt, dimensionality);
      } else {
        CLINT_UNREACHABLE;
      }
    } else if (vcs->verticalDimensionIdx() == VizProperties::NO_DIMENSION) {
      if (dimensionality >= m_verticalDimensionIdx) {
        // Target projection does not have vertical dimension, but the polyhedron has and the projection covers it.
        // -> create new cs
        csAt.m_action = IsCsAction::InsertCS;
        return insertCs(csAt, dimensionality);
      } else {
        // Do nothing (invisible in this projection)
        CLINT_UNREACHABLE;
      }
    } else if (dimensionality <= m_horizontalDimensionIdx) {
      csAt.m_action = IsCsAction::InsertPile;
      return insertPile(csAt, dimensionality);
    } else if (dimensionality <= m_verticalDimensionIdx) {
      // Target projection does have a dimension that the polyhedron does not.
      // -> create new cs
      csAt.m_action = IsCsAction::InsertCS;
      return insertCs(csAt, dimensionality);
    }
    return vcs;
    break;
  case IsCsAction::InsertPile:
    return insertPile(csAt, dimensionality);
    break;
  case IsCsAction::InsertCS:
    if (m_coordinateSystems[csAt.pileIdx()].size() == 1 && // No loop in this pile
        !m_coordinateSystems[csAt.pileIdx()][0]->isHorizontalAxisVisible()) {
      csAt.m_action = IsCsAction::InsertPile;
      return insertPile(csAt, dimensionality);
    }
    return insertCs(csAt, dimensionality);
    break;
  }
  CLINT_UNREACHABLE;
}

void VizProjection::deleteCoordinateSystem(VizCoordinateSystem *vcs) {
  size_t pileIdx, csIdx;
  std::tie(pileIdx, csIdx) = csIndices(vcs);
  CLINT_ASSERT(pileIdx != static_cast<size_t>(-1),
               "Coordinate sytem does not belong to the projection it is being removed from");
  m_coordinateSystems[pileIdx].erase(std::begin(m_coordinateSystems[pileIdx]) + csIdx);
  if (m_coordinateSystems[pileIdx].empty()) {
    m_coordinateSystems.erase(std::begin(m_coordinateSystems) + pileIdx);
  }
  vcs->setVisible(false);
  vcs->setParentItem(nullptr);

  CLINT_ASSERT(vcs->polyhedra().size() == 0, "deleting non-empty CS");

  vcs->deleteLater();
//  delete vcs; // No parent now, so delete.
  updateSceneLayout();
}

/// Return first non-empty coordinate system in the pile that does not contain the given polyhedron.
VizCoordinateSystem *VizProjection::firstNonEmptyCoordinateSystem(size_t pileIdx, VizPolyhedron *except) const {
  if (pileIdx >= m_coordinateSystems.size())
    return nullptr;

  for (VizCoordinateSystem *vcs : m_coordinateSystems[pileIdx]) {
    const std::vector<VizPolyhedron *> &polyhedra = vcs->polyhedra();
    if (!polyhedra.empty()) {
      if (std::find(std::begin(polyhedra), std::end(polyhedra), except) == std::end(polyhedra)) {
        return vcs;
      }
    }
  }
  return nullptr;
}

inline bool partialBetaEquals(const std::vector<int> &original, const std::vector<int> &beta,
                              size_t from, size_t to) {
  return std::equal(std::begin(original) + from,
                    std::begin(original) + to,
                    std::begin(beta) + from);
}

VizCoordinateSystem *VizProjection::createCoordinateSystem(int dimensionality) {
  VizCoordinateSystem *vcs;
  vcs = new VizCoordinateSystem(this,
                                m_horizontalDimensionIdx < dimensionality ?
                                  m_horizontalDimensionIdx :
                                  VizProperties::NO_DIMENSION,
                                m_verticalDimensionIdx < dimensionality ?
                                  m_verticalDimensionIdx :
                                  VizProperties::NO_DIMENSION);

  m_scene->addItem(vcs);
  return vcs;
}

void VizProjection::appendCoordinateSystem(int dimensionality) {
  VizCoordinateSystem *vcs = createCoordinateSystem(dimensionality);
  m_coordinateSystems.back().push_back(vcs);
}

void VizProjection::updateOuterDependences() {
  // This is veeeery inefficient.
  for (int pileIdx = 0, pileIdxEnd = m_coordinateSystems.size(); pileIdx < pileIdxEnd; pileIdx++) {
    const std::vector<VizCoordinateSystem *> &pile1 = m_coordinateSystems[pileIdx];
    for (int csIdx = 0, csIdxEnd = pile1.size(); csIdx < csIdxEnd; csIdx++) {
      VizCoordinateSystem *cs1 = pile1[csIdx];
      cs1->nextCsIsDependent = 0;
      cs1->nextPileIsDependent = 0;

      if (csIdx < csIdxEnd - 1) {
        VizCoordinateSystem *cs2 = pile1[csIdx + 1];
        int r = cs1->dependentWith(cs2);
        if (r > 0) {
          cs1->nextCsIsDependent = true;
        }
        if (r > 1) {
          cs1->nextCsIsViolated = true;
        }
      }
      if (pileIdx < pileIdxEnd - 1) {
        const std::vector<VizCoordinateSystem *> &pile2 = m_coordinateSystems[pileIdx + 1];
        for (int csoIdx = 0, csoIdxEnd = pile2.size(); csoIdx < csoIdxEnd; csoIdx++) {
          VizCoordinateSystem *cs3 = pile2[csoIdx];
          int r = cs1->dependentWith(cs3);
          if (r > 0) {
            pile1.front()->nextPileIsDependent = true;
          }
          if (r > 1) {
            pile1.front()->nextPileIsViolated = true;
          }
        }
      }
    }
  }
}

void VizProjection::updateInnerDependences() {
  for (auto pile : m_coordinateSystems) {
    for (VizCoordinateSystem *vcs : pile) {
      vcs->updateInnerDependences();
    }
  }
}

void VizProjection::updateInternalDependences() {
  for (auto pile : m_coordinateSystems) {
    for (VizCoordinateSystem *vcs : pile) {
      vcs->updateInternalDependences();
    }
  }
}

static int firstDifferentDimension(const std::vector<int> &beta1, const std::vector<int> &beta2) {
  // With beta-vectors for statements, we cannot have a match that is not equality,
  // i.e. we cannot have simultaneously [1] and [1,3] as beta-vectors for statements.
  CLINT_ASSERT(!std::equal(std::begin(beta1),
                           std::begin(beta1) + std::min(beta1.size(), beta2.size()),
                           std::begin(beta2)),
               "One statement occurence corresponds to the beta-prefix (loop)");
  auto mismatchIterators =
      std::mismatch(std::begin(beta1), std::end(beta1),
      std::begin(beta2), std::end(beta2));
  int difference = mismatchIterators.first - std::begin(beta1);
  return difference;
}

void VizProjection::projectScop() {
  m_selectionManager->clearSelection();
  for (int i = 0; i < m_coordinateSystems.size(); i++) {
    for (int j = 0; j < m_coordinateSystems[i].size(); j++) {
      delete m_coordinateSystems[i][j];
    }
  }
  m_coordinateSystems.clear();

  // With beta-vectors for statements, we cannot have a match that is not equality,
  // i.e. we cannot have simultaneously [1] and [1,3] as beta-vectors for statements.
  // Therefore when operating with statements, any change in beta-vector equality
  // results in a new container creation.
  std::vector<ClintStmtOccurrence *> allOccurrences;
  for (ClintStmt *vstmt : m_scop->statements()) {
    std::vector<ClintStmtOccurrence *> stmtOccurrences = vstmt->occurrences();
    allOccurrences.insert(std::end(allOccurrences),
                          std::make_move_iterator(std::begin(stmtOccurrences)),
                          std::make_move_iterator(std::end(stmtOccurrences)));
  }
  std::sort(std::begin(allOccurrences), std::end(allOccurrences), VizStmtOccurrencePtrComparator());

  VizCoordinateSystem *vcs              = nullptr;
  ClintStmtOccurrence *previousOccurrence = nullptr;
  bool visiblePile   = true;
  bool visibleCS     = true;
  std::vector<std::pair<int, int>> columnMinMax;
  std::vector<std::vector<std::pair<int, int>>> csMinMax;
  int horizontalMin = 0;
  int horizontalMax = 0;
  int verticalMin   = 0;
  int verticalMax   = 0;
  for (ClintStmtOccurrence *occurrence : allOccurrences) {
    int difference = previousOccurrence ?
          firstDifferentDimension(previousOccurrence->untiledBetaVector(),
                                  occurrence->untiledBetaVector()) :
          -1;
    if (difference < m_horizontalDimensionIdx + 1 &&
        visiblePile) {
      m_coordinateSystems.emplace_back();
      columnMinMax.emplace_back(std::make_pair(INT_MAX, INT_MIN));
      csMinMax.emplace_back();
      csMinMax.back().emplace_back(std::make_pair(INT_MAX, INT_MIN));
      appendCoordinateSystem(occurrence->dimensionality());
      visiblePile = false;
      visibleCS = false;
    } else if (difference < m_verticalDimensionIdx + 1 &&
               visibleCS) {
      appendCoordinateSystem(occurrence->dimensionality());
      csMinMax.back().emplace_back(std::make_pair(INT_MAX, INT_MIN));
      visibleCS = false;
    }
    vcs = m_coordinateSystems.back().back();
    visibleCS = vcs->projectStatementOccurrence(occurrence) || visibleCS;
    visiblePile = visiblePile || visibleCS;
    if (m_horizontalDimensionIdx != VizProperties::NO_DIMENSION) {
      horizontalMin = occurrence->minimumValue(m_horizontalDimensionIdx);
      horizontalMax = occurrence->maximumValue(m_horizontalDimensionIdx);
    }
    if (m_verticalDimensionIdx != VizProperties::NO_DIMENSION) {
      verticalMin = occurrence->minimumValue(m_verticalDimensionIdx);
      verticalMax = occurrence->maximumValue(m_verticalDimensionIdx);
    }
    std::pair<int, int> minmax = columnMinMax.back();
    minmax.first  = std::min(minmax.first, horizontalMin);
    minmax.second = std::max(minmax.second, horizontalMax);
    columnMinMax.back() = minmax;
    minmax = csMinMax.back().back();
    minmax.first  = std::min(minmax.first, verticalMin);
    minmax.second = std::max(minmax.second, verticalMax);
    csMinMax.back().back() = minmax;
    previousOccurrence = occurrence;
  }
  CLINT_ASSERT(columnMinMax.size() == m_coordinateSystems.size(),
               "Sizes of column min/maxes and coordinate system columns don't match");
  CLINT_ASSERT(csMinMax.size() == m_coordinateSystems.size(),
               "Sizes of coordinate systems min/maxes don't match");
  for (size_t col = 0, colend = m_coordinateSystems.size(); col < colend; col++) {
    CLINT_ASSERT(csMinMax[col].size() == m_coordinateSystems[col].size(),
                 "Sizes of coordinate systems min/maxes don't match");
    for (size_t row = 0, rowend = m_coordinateSystems[col].size(); row < rowend; row++) {
      VizCoordinateSystem *vcs = m_coordinateSystems[col][row];
      vcs->setMinMax(columnMinMax[col], csMinMax[col][row]);
    }
  }
  if (!visibleCS) {
    VizCoordinateSystem *vcs = m_coordinateSystems.back().back();
    m_scene->removeItem(vcs);
    m_coordinateSystems.back().pop_back();
    delete vcs;
  }
  if (!visiblePile) {
    m_coordinateSystems.pop_back();
  }

  updateOuterDependences();

  updateInnerDependences();

  updateSceneLayout();
}

VizPolyhedron *VizProjection::polyhedron(ClintStmtOccurrence *occurrence) const {
  for (auto pile : m_coordinateSystems) {
    for (VizCoordinateSystem *vcs : pile) {
      for (VizPolyhedron *ph : vcs->polyhedra()) {
        if (ph->occurrence() == occurrence)
          return ph;
      }
    }
  }
  return nullptr;
}

std::vector<int> csFirstPolyhedronBetaVector(const VizCoordinateSystem *const &vcs) {
  CLINT_ASSERT(vcs->polyhedra().size() != 0,
               "empty coordinate system present");
  VizPolyhedron *vph = vcs->polyhedra().at(0);
  return vph->occurrence()->betaVector();
}

std::vector<int> pileFirstPolyhedronBetaVector(const std::vector<VizCoordinateSystem *> &pile) {
  CLINT_ASSERT(pile.size() != 0,
               "empty pile present");
  const VizCoordinateSystem *vcs = pile[0];
  return csFirstPolyhedronBetaVector(vcs);
}

VizCoordinateSystem * VizProjection::createCoordinateSystem(VizCoordinateSystem *oldVCS) {
  // Use this to create the coordiante system with axes disabled.  Since we
  // do not know the dimensionality as this point, we set it to the vertical
  // dimension index or horizontal dimension index if vertical is invisible.
  int fakeDimensionality = 0;
  if (oldVCS->verticalDimensionIdx() != VizProperties::NO_DIMENSION) {
    fakeDimensionality = oldVCS->verticalDimensionIdx() + 1;
  } else if (oldVCS->horizontalDimensionIdx() != VizProperties::NO_DIMENSION) {
    fakeDimensionality = oldVCS->horizontalDimensionIdx() + 1;
  }
  VizCoordinateSystem *newCS = createCoordinateSystem(fakeDimensionality);

  return newCS;
}

void VizProjection::reflectBetaTransformation(const Transformation &transformation) {
  // When this function is called, betas in the occurrences are not updated yet.
  if (m_skipBetaTransformations != 0) {
    --m_skipBetaTransformations;
    return;
  }

  if (transformation.kind() == Transformation::Kind::Split) {
    std::vector<int> beta = transformation.target();
    std::unordered_set<ClintStmtOccurrence *> occs = m_scop->occurrences(beta);
    CLINT_ASSERT(occs.size() != 0,
                 "Couldn't find occurrence from which to split away");
    ClintStmtOccurrence *occurrence = *occs.begin();
    VizCoordinateSystem *vcs = polyhedron(occurrence)->coordinateSystem();
    size_t pileIdx, csIdx;
    std::tie(pileIdx, csIdx) = csIndices(vcs);
    int dimension = transformation.target().size() - 2;

    if (dimension == m_horizontalDimensionIdx) { // TODO: 1-d occurrences are ok here?
      // move CSs into the new pile
      CLINT_ASSERT(m_coordinateSystems[pileIdx].size() > csIdx + 1,
                   "No CSs to split away");

      // Recreate coordinate systems instead of just moving the existing ones in order
      // to reuse animated polyhedron reparenting functionality.
      m_coordinateSystems.insert(std::begin(m_coordinateSystems) + pileIdx + 1, std::vector<VizCoordinateSystem *>());
      for (size_t i = csIdx + 1; i < m_coordinateSystems[pileIdx].size(); ++i) {
        VizCoordinateSystem *oldVCS = m_coordinateSystems[pileIdx][i];
        VizCoordinateSystem *newVCS = createCoordinateSystem(oldVCS);
        m_coordinateSystems[pileIdx + 1].push_back(newVCS);
        for (VizPolyhedron *vph : oldVCS->polyhedra()) {
          newVCS->reparentPolyhedron(vph);
        }
        deleteCoordinateSystem(oldVCS);
      }
    } else if (dimension == m_verticalDimensionIdx) {
      // move polyhedra into a new CS
      VizCoordinateSystem *cs = createCoordinateSystem(occurrence->dimensionality());
      m_coordinateSystems[pileIdx].insert(std::begin(m_coordinateSystems[pileIdx]) + csIdx + 1,
                                          cs);

      std::vector<int> betaPrefix(std::begin(beta), std::end(beta) - 1);
      for (VizPolyhedron *vph : vcs->polyhedra()) {
        const std::vector<int> &phBeta = vph->occurrence()->betaVector();
        if (BetaUtility::isPrefix(betaPrefix, phBeta) &&
            BetaUtility::follows(beta, phBeta)) {
          cs->reparentPolyhedron(vph);
        }
      }
    }

  } else if (transformation.kind() == Transformation::Kind::Fuse) {
    std::vector<int> beta = transformation.target();
    std::unordered_set<ClintStmtOccurrence *> occs = m_scop->occurrences(beta);
    CLINT_ASSERT(occs.size() != 0,
                 "Couldn't find a CS to fuse with");
    ClintStmtOccurrence *occurrence = *occs.begin();
    VizCoordinateSystem *vcs = polyhedron(occurrence)->coordinateSystem();
    size_t pileIdx, csIdx;
    std::tie(pileIdx, csIdx) = csIndices(vcs);
    int dimension = transformation.target().size() - 1;

    if (dimension == m_horizontalDimensionIdx) {
      // add contents of next pile to the current pile
      CLINT_ASSERT(m_coordinateSystems.size() > pileIdx + 1,
                   "No pile to fuse");

      // Recreate coordinate systems instead of just moving the existing ones in order
      // to reuse animated polyhedron reparenting functionality.
      for (size_t i = 0, e = m_coordinateSystems[pileIdx + 1].size(); i < e; ++i) {
        VizCoordinateSystem *oldVCS = m_coordinateSystems[pileIdx + 1].front();
        VizCoordinateSystem *newVCS = createCoordinateSystem(oldVCS);
        m_coordinateSystems[pileIdx].push_back(newVCS);
        for (VizPolyhedron *vph : oldVCS->polyhedra()) {
          newVCS->reparentPolyhedron(vph);
        }
        deleteCoordinateSystem(oldVCS);
      }

    } else if (dimension == m_verticalDimensionIdx) {
      // add contents of the next coordinate system to the current
      CLINT_ASSERT(m_coordinateSystems[pileIdx].size() > csIdx + 1,
                   "No CS to fuse");
      VizCoordinateSystem *cs = m_coordinateSystems[pileIdx][csIdx + 1];
      for (VizPolyhedron *vph : cs->polyhedra()) {
        vcs->reparentPolyhedron(vph);
      }
      deleteCoordinateSystem(cs);
    }
  } else if (transformation.kind() == Transformation::Kind::Reorder) {
    int dimension = transformation.target().size();
    if (dimension <= m_horizontalDimensionIdx) {
      // reorder piles
      m_coordinateSystems = reflectReorder<std::vector<VizCoordinateSystem *>>(
            m_coordinateSystems, pileFirstPolyhedronBetaVector,
            transformation);
    } else if (dimension <= m_verticalDimensionIdx) {
      // reorder CSs
      for (size_t pileIdx = 0; pileIdx < m_coordinateSystems.size(); ++pileIdx) {
        if (BetaUtility::isPrefix(transformation.target(),
                                  pileFirstPolyhedronBetaVector(m_coordinateSystems[pileIdx]))) {
          m_coordinateSystems[pileIdx] = reflectReorder<VizCoordinateSystem *>(
                m_coordinateSystems[pileIdx], csFirstPolyhedronBetaVector,
                transformation);
        }
      }
    } else {
      // reorder polyhedra
      for (size_t pileIdx = 0; pileIdx < m_coordinateSystems.size(); ++pileIdx) {
        for (size_t csIdx = 0; csIdx < m_coordinateSystems[pileIdx].size(); ++csIdx) {
          VizCoordinateSystem *vcs = m_coordinateSystems[pileIdx][csIdx];
          if (BetaUtility::isPrefix(transformation.target(),
                                    vcs->betaPrefix())) {
            vcs->reorderPolyhedra(transformation);
          }
        }
      }
    }
  } else if (transformation.kind() == Transformation::Kind::Embed) {
    std::vector<int> beta  = transformation.target();
    std::unordered_set<ClintStmtOccurrence *> occs = m_scop->occurrences(beta);
    CLINT_ASSERT(occs.size() != 0,
                 "Coundn't find target occurrence");
    ClintStmtOccurrence *occurrence = *occs.begin();
    VizCoordinateSystem *vcs = polyhedron(occurrence)->coordinateSystem();
    size_t pileIdx, csIdx;
    std::tie(pileIdx, csIdx) = csIndices(vcs);
    int dimension = transformation.target().size() - 1;
    if (m_horizontalDimensionIdx == dimension &&
        !vcs->isHorizontalAxisVisible()) {
      vcs->setHorizontalAxisState(VizCoordinateSystem::AxisState::Visible);
      vcs->update();
    } else if (m_verticalDimensionIdx == dimension &&
               !vcs->isVerticalAxisVisible()) {
      vcs->setVerticalAxisState(VizCoordinateSystem::AxisState::Visible);
    }
  } else if (transformation.kind() == Transformation::Kind::Unembed) {
    std::vector<int> beta  = transformation.target();
    std::unordered_set<ClintStmtOccurrence *> occs = m_scop->occurrences(beta);
    CLINT_ASSERT(occs.size() != 0,
                 "Coundn't find target occurrence");
    ClintStmtOccurrence *occurrence = *occs.begin();
    VizCoordinateSystem *vcs = polyhedron(occurrence)->coordinateSystem();
    size_t pileIdx, csIdx;
    std::tie(pileIdx, csIdx) = csIndices(vcs);
    int dimension = transformation.target().size() - 2;
    if (m_horizontalDimensionIdx == dimension &&
        vcs->isHorizontalAxisVisible()) {
      vcs->setHorizontalAxisState(VizCoordinateSystem::AxisState::Invisible);
    } else if (m_verticalDimensionIdx == dimension &&
               vcs->isVerticalAxisVisible()) {
      vcs->setVerticalAxisState(VizCoordinateSystem::AxisState::Invisible);
    }
  }
}

void VizProjection::updateColumnHorizontalMinMax(VizCoordinateSystem *coordinateSystem, int minOffset, int maxOffset) {
  size_t column = static_cast<size_t>(-1);
  int columnMin;
  int columnMax;
  for (size_t col = 0, col_end = m_coordinateSystems.size(); col < col_end; col++) {
    std::vector<VizCoordinateSystem *> pile = m_coordinateSystems.at(col);
    columnMin = INT_MAX;
    columnMax = INT_MIN;
    for (size_t row = 0, row_end = pile.size(); row < row_end; row++) {
      int hmin, hmax, vmin, vmax;
      VizCoordinateSystem *cs = pile.at(row);
      cs->minMax(hmin, hmax, vmin, vmax);
      if (cs == coordinateSystem) {
        column = col;
        hmin += minOffset; // FIXME: coordinate system may contain mulitple polyhedra, only one of which is moved...
        hmax += maxOffset;
      }
      columnMin = std::min(columnMin, hmin);
      columnMax = std::max(columnMax, hmax);
    }
    // Found, no further iteration needed.
    if (column == col) {
      break;
    }
  }
  CLINT_ASSERT(column != static_cast<size_t>(-1), "Polyhedron not found in the coordinate system");

  std::vector<VizCoordinateSystem *> pile = m_coordinateSystems.at(column);
  for (size_t row = 0, row_end = pile.size(); row < row_end; row++) {
    VizCoordinateSystem *cs = pile.at(row);
    int hmin, hmax, vmin, vmax;
    cs->minMax(hmin, hmax, vmin, vmax);
    cs->setMinMax(columnMin, columnMax, vmin, vmax);
  }
  updateProjection();
}

void VizProjection::ensureFitsHorizontally(VizCoordinateSystem *coordinateSystem, int minimum, int maximum) {
  size_t column = static_cast<size_t>(-1);
  int columnMin;
  int columnMax;
  for (size_t col = 0, col_end = m_coordinateSystems.size(); col < col_end; col++) {
    std::vector<VizCoordinateSystem *> pile = m_coordinateSystems.at(col);
    columnMin = INT_MAX;
    columnMax = INT_MIN;
    for (size_t row = 0, row_end = pile.size(); row < row_end; row++) {
      int hmin, hmax, vmin, vmax;
      VizCoordinateSystem *cs = pile.at(row);
      cs->minMax(hmin, hmax, vmin, vmax);
      if (cs == coordinateSystem) {
        column = col;
      }
      columnMin = std::min(columnMin, hmin);
      columnMax = std::max(columnMax, hmax);
    }
    // Found, no further iteration needed.
    if (column == col) {
      break;
    }
  }
  CLINT_ASSERT(column != static_cast<size_t>(-1), "Polyhedron not found in the coordinate system");

  columnMin = std::min(columnMin, minimum);
  columnMax = std::max(columnMax, maximum);

  std::vector<VizCoordinateSystem *> pile = m_coordinateSystems.at(column);
  for (size_t row = 0, row_end = pile.size(); row < row_end; row++) {
    VizCoordinateSystem *cs = pile.at(row);
    cs->setHorizontalMinMax(columnMin, columnMax);
  }
  updateProjection();
}

void VizProjection::ensureFitsVertically(VizCoordinateSystem *coordinateSystem, int minimum, int maximum) {
  int hmin, hmax, vmin, vmax;
  coordinateSystem->minMax(hmin, hmax, vmin, vmax);
  vmin = std::min(vmin, minimum);
  vmax = std::max(vmax, maximum);
  coordinateSystem->setMinMax(hmin, hmax, vmin, vmax);

  updateProjection();
}

void VizProjection::updateSceneLayout() {
  double horizontalOffset = 0.0;
  for (size_t col = 0, col_end = m_coordinateSystems.size(); col < col_end; col++) {
    double verticalOffset = 0.0;
    double maximumWidth = 0.0;
    double left = HUGE_VAL;
    for (size_t row = 0, row_end = m_coordinateSystems[col].size(); row < row_end; row++) {
      VizCoordinateSystem *vcs = m_coordinateSystems[col][row];
      QRectF bounding = vcs->boundingRect();
      left = std::min(left, bounding.left());
    }

    for (size_t row = 0, row_end = m_coordinateSystems[col].size(); row < row_end; row++) {
      VizCoordinateSystem *vcs = m_coordinateSystems[col][row];
      QRectF bounding = vcs->boundingRect();
      vcs->setPos(horizontalOffset - left, -verticalOffset - bounding.bottom());
      verticalOffset += bounding.height() + m_vizProperties->coordinateSystemMargin();
      maximumWidth = std::max(maximumWidth, bounding.width());
    }
    horizontalOffset += maximumWidth + m_vizProperties->coordinateSystemMargin();
  }
  updateOuterDependences();
}

void VizProjection::selectProjection() {
  emit selected(m_horizontalDimensionIdx, m_verticalDimensionIdx);
}

VizProjection::IsCsResult VizProjection::emptyIsCsResult() const {
  IsCsResult r;
  r.m_action = IsCsAction::Found;
  r.m_coordinateSystem = static_cast<size_t>(-1);
  r.m_pile = static_cast<size_t>(-1);
  r.m_vcs = nullptr;
  return r;
}

void VizProjection::showInsertionShadow(IsCsResult r) {
  const double csMargin = vizProperties()->coordinateSystemMargin();
  if (m_previousIsCsResult != r) {
    if (m_insertionShadow) {
      m_scene->removeItem(m_insertionShadow);
      delete m_insertionShadow;
      m_insertionShadow = nullptr;
    }
    m_previousIsCsResult = r;
  } else {
    return;
  }

  if (r.action() == IsCsAction::InsertCS) {
    double top = 0.0;
    double left = 0.0;
    double width = 0.0;
    size_t pileIdx = r.pileIdx();
    VizCoordinateSystem *vcs;
    if (r.coordinateSystemIdx() == m_coordinateSystems[pileIdx].size()) {
      vcs = m_coordinateSystems[pileIdx].back();
      top = vcs->pos().y() + vcs->outerRect().top() - csMargin;
    } else {
      vcs = m_coordinateSystems[pileIdx][r.coordinateSystemIdx()];
      top = vcs->pos().y() + vcs->outerRect().bottom();
    }
    QRectF csRect = vcs->outerRect();
    left = vcs->pos().x() + csRect.left();
    width = csRect.width();
    QGraphicsRectItem *rect = new QGraphicsRectItem(left, top, width, csMargin);
    rect->setBrush(QBrush(Qt::lightGray, Qt::DiagCrossPattern));
    rect->setPen(QPen(Qt::white));
    m_insertionShadow = rect;
    m_scene->addItem(rect);

  } else if (r.action() == IsCsAction::InsertPile) {
    double left;
    double top;
    double height;
    VizCoordinateSystem *vcs;
    if (r.pileIdx() == m_coordinateSystems.size()) {
      vcs = m_coordinateSystems.back().front();
      left = vcs->pos().x() + vcs->outerRect().right();
    } else {
      vcs = m_coordinateSystems[r.pileIdx()].front();
      left = vcs->pos().x() + vcs->outerRect().left() - csMargin;
    }
    QRectF csRect = vcs->outerRect();
    top = vcs->pos().y() + csRect.top();
    height = csRect.height();
    QGraphicsRectItem *rect = new QGraphicsRectItem(left, top, csMargin, height);
    rect->setBrush(QBrush(Qt::lightGray, Qt::DiagCrossPattern));
    rect->setPen(QPen(Qt::white));
    m_insertionShadow = rect;
    m_scene->addItem(rect);
  }
}
