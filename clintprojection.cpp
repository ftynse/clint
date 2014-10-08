#include "clintprojection.h"
#include "oslutils.h"
#include "vizcoordinatesystem.h"
#include "vizstmtoccurrence.h"

#include <QtWidgets>
#include <QtCore>

#include <algorithm>
#include <map>
#include <vector>

// FIXME: hardcoded margin value
const double coordinateSystemMargin = 5.0;

ClintProjection::ClintProjection(int horizontalDimensionIdx, int verticalDimensionIdx, QObject *parent) :
  QObject(parent), m_horizontalDimensionIdx(horizontalDimensionIdx), m_verticalDimensionIdx(verticalDimensionIdx) {

  m_scene = new QGraphicsScene;
  m_view = new QGraphicsView(m_scene);
}

inline bool partialBetaEquals(const std::vector<int> &original, const std::vector<int> &beta,
                              size_t from, size_t to) {
  return std::equal(std::begin(original) + from,
                    std::begin(original) + to,
                    std::begin(beta) + from);
}

void ClintProjection::createCoordinateSystem(int dimensionality) {
  VizCoordinateSystem *vcs;
  vcs = new VizCoordinateSystem(m_horizontalDimensionIdx < dimensionality ?
                                  m_horizontalDimensionIdx :
                                  VizCoordinateSystem::NO_DIMENSION,
                                m_verticalDimensionIdx < dimensionality ?
                                  m_verticalDimensionIdx :
                                  VizCoordinateSystem::NO_DIMENSION);
  m_coordinateSystems.back().push_back(vcs);
  m_scene->addItem(vcs);
}

void ClintProjection::projectScop(VizScop *vscop) {
  // With beta-vectors for statements, we cannot have a match that is not equality,
  // i.e. we cannot have simultaneously [1] and [1,3] as beta-vectors for statements.
  // Therefore when operating with statements, any change in beta-vector equality
  // results in a new container creation.
  std::vector<VizStmtOccurrence *> allOccurrences;
  for (VizStatement *vstmt : vscop->statements()) {
    std::vector<VizStmtOccurrence *> stmtOccurrences = vstmt->occurences();
    allOccurrences.insert(std::end(allOccurrences),
                          std::make_move_iterator(std::begin(stmtOccurrences)),
                          std::make_move_iterator(std::end(stmtOccurrences)));
  }
  std::sort(std::begin(allOccurrences), std::end(allOccurrences), VizStmtOccurrencePtrComparator());

  VizCoordinateSystem *vcs              = nullptr;
  VizStmtOccurrence *previousOccurrence = nullptr;
  bool visiblePile   = true;
  bool visibleCS     = true;
  std::vector<std::pair<int, int>> columnMinMax;
  std::vector<std::vector<std::pair<int, int>>> csMinMax;
  int horizontalMin = 0;
  int horizontalMax = 0;
  int verticalMin   = 0;
  int verticalMax   = 0;
  for (VizStmtOccurrence *occurrence : allOccurrences) {
    int difference = previousOccurrence ?
          previousOccurrence->firstDifferentDimension(*occurrence) :
          -1;
    if (difference < m_horizontalDimensionIdx + 1 &&
        visiblePile) {
      m_coordinateSystems.emplace_back();
      columnMinMax.emplace_back(std::make_pair(INT_MAX, INT_MIN));
      csMinMax.emplace_back();
      csMinMax.back().emplace_back(std::make_pair(INT_MAX, INT_MIN));
      createCoordinateSystem(occurrence->dimensionality());
      visiblePile = false;
      visibleCS = false;
    } else if (difference < m_verticalDimensionIdx + 1 &&
               visibleCS) {
      createCoordinateSystem(occurrence->dimensionality());
      csMinMax.back().emplace_back(std::make_pair(INT_MAX, INT_MIN));
      visibleCS = false;
    }
    vcs = m_coordinateSystems.back().back();
    visibleCS = vcs->projectStatementOccurrence(occurrence) || visibleCS;
    visiblePile = visiblePile || visibleCS;
    if (m_horizontalDimensionIdx != VizCoordinateSystem::NO_DIMENSION) {
      horizontalMin = occurrence->minimumValue(m_horizontalDimensionIdx);
      horizontalMax = occurrence->maximumValue(m_horizontalDimensionIdx);
    }
    if (m_verticalDimensionIdx != VizCoordinateSystem::NO_DIMENSION) {
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
  updateSceneLayout();
}

void ClintProjection::updateSceneLayout() {
  double horizontalOffset = 0.0;
  for (size_t col = 0, col_end = m_coordinateSystems.size(); col < col_end; col++) {
    double verticalOffset = 0.0;
    double maximumWidth = 0.0;
    for (size_t row = 0, row_end = m_coordinateSystems[col].size(); row < row_end; row++) {
      VizCoordinateSystem *vcs = m_coordinateSystems[col][row];
      QRectF bounding = vcs->boundingRect();
      vcs->setPos(horizontalOffset, -verticalOffset);
      verticalOffset += bounding.height() + coordinateSystemMargin;
      maximumWidth = std::max(maximumWidth, bounding.width());
    }
    horizontalOffset += maximumWidth + coordinateSystemMargin;
  }
}
