#include "vizprojection.h"
#include "oslutils.h"
#include "vizcoordinatesystem.h"
#include "clintstmtoccurrence.h"

#include <QtWidgets>
#include <QtCore>

#include <algorithm>
#include <map>
#include <vector>

VizProjection::VizProjection(int horizontalDimensionIdx, int verticalDimensionIdx, QObject *parent) :
  QObject(parent), m_horizontalDimensionIdx(horizontalDimensionIdx), m_verticalDimensionIdx(verticalDimensionIdx) {

  m_scene = new QGraphicsScene;
  m_view = new QGraphicsView(m_scene);

  m_view->setDragMode(QGraphicsView::RubberBandDrag);
  m_view->setRubberBandSelectionMode(Qt::ContainsItemShape);

  m_vizProperties = new VizProperties(this);
  connect(m_vizProperties, &VizProperties::vizPropertyChanged,
          this, &VizProjection::updateProjection);

  m_selectionManager = new VizSelectionManager(this);
  m_manipulationManager = new VizManipulationManager(this);
}

void VizProjection::updateProjection() {
  updateSceneLayout();
  m_view->viewport()->update();
}

VizProjection::IsCsResult VizProjection::isCoordinateSystem(QPointF point) {
  bool found = false;
  size_t pileIndex = static_cast<size_t>(-1);
  IsCsResult result;
  for (size_t i = 0, ei = m_coordinateSystems.size(); i < ei; i++) {
    const std::vector<VizCoordinateSystem *> &pile = m_coordinateSystems.at(i);
    VizCoordinateSystem *coordinateSystem = pile.front();
    if (point.x() < coordinateSystem->pos().x()) {
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
        double csRight = previousCS->pos().x() + csRect.width();
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
    double csRight = coordinateSystem->pos().x() + csRect.width();
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
    if (point.y() > coordinateSystem->pos().y()) {
      if (i == 0) {
        // add before first cs
        result.m_action = IsCsAction::InsertCS;
        result.m_coordinateSystem = 0;
        return result;
      } else {
        VizCoordinateSystem *previousCS = pile.at(i - 1);
        QRectF csRect = previousCS->coordinateSystemRect();
        double csTop = previousCS->pos().y() - csRect.height();
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
  double csTop = coordinateSystem->pos().y() - csRect.height();
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

VizCoordinateSystem *VizProjection::insertPile(IsCsResult csAt, int dimensionality) {
  VizCoordinateSystem *vcs = createCoordinateSystem(dimensionality);
  m_coordinateSystems.insert(std::next(std::begin(m_coordinateSystems), csAt.pileIdx()),
                             std::vector<VizCoordinateSystem *> {vcs});
  updateProjection();
  return vcs;
}

VizCoordinateSystem *VizProjection::insertCs(IsCsResult csAt, int dimensionality) {
  CLINT_ASSERT(csAt.pileIdx() < m_coordinateSystems.size(), "Inserting CS in a non-existent pile");
  VizCoordinateSystem *vcs = createCoordinateSystem(dimensionality);
  std::vector<VizCoordinateSystem *> &pile = m_coordinateSystems.at(csAt.pileIdx());
  pile.insert(std::next(std::begin(pile), csAt.coordinateSystemIdx()), vcs);
  updateProjection();
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
    if (dimensionality <= m_horizontalDimensionIdx) {
      csAt.m_action = IsCsAction::InsertPile;
      return insertPile(csAt, dimensionality);
    } else if (dimensionality <= m_verticalDimensionIdx) {
      // Target projection does have a dimension that the polyhedron does not.
      // -> create new cs
      csAt.m_action = IsCsAction::InsertCS;
      return insertCs(csAt, dimensionality);
    } else if (vcs->horizontalDimensionIdx() == VizProperties::NO_DIMENSION) {
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
    }
    return vcs;
    break;
  case IsCsAction::InsertPile:
    return insertPile(csAt, dimensionality);
    break;
  case IsCsAction::InsertCS:
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
  vcs->setParentItem(nullptr);
  m_coordinateSystems[pileIdx].erase(std::begin(m_coordinateSystems[pileIdx]) + csIdx);
  if (m_coordinateSystems[pileIdx].empty()) {
    m_coordinateSystems.erase(std::begin(m_coordinateSystems) + pileIdx);
  }
  delete vcs; // No parent now, so delete.
  updateSceneLayout();
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

void VizProjection::projectScop(ClintScop *vscop) {
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
  for (ClintStmt *vstmt : vscop->statements()) {
    std::vector<ClintStmtOccurrence *> stmtOccurrences = vstmt->occurences();
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
          previousOccurrence->firstDifferentDimension(*occurrence) :
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
    int hmin, hmax, vmin, vmax;
    cs->minMax(hmin, hmax, vmin, vmax);
    cs->setMinMax(columnMin, columnMax, vmin, vmax); // FIXME: provide functionality for horizontalMinMax separately
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
    for (size_t row = 0, row_end = m_coordinateSystems[col].size(); row < row_end; row++) {
      VizCoordinateSystem *vcs = m_coordinateSystems[col][row];
      QRectF bounding = vcs->boundingRect();
      vcs->setPos(horizontalOffset, -verticalOffset);
      verticalOffset += bounding.height() + m_vizProperties->coordinateSystemMargin();
      maximumWidth = std::max(maximumWidth, bounding.width());
    }
    horizontalOffset += maximumWidth + m_vizProperties->coordinateSystemMargin();
  }
}
