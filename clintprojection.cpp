#include "clintprojection.h"
#include "vizcoordinatesystem.h"
#include "oslutils.h"

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

void ClintProjection::createCoordinateSystem(const std::vector<int> &betaVector) {
  VizCoordinateSystem *vcs;
  vcs = new VizCoordinateSystem(m_horizontalDimensionIdx < (betaVector.size() - 1) ?
                                  m_horizontalDimensionIdx :
                                  VizCoordinateSystem::NO_DIMENSION,
                                m_verticalDimensionIdx < (betaVector.size() - 1) ?
                                  m_verticalDimensionIdx :
                                  VizCoordinateSystem::NO_DIMENSION);
  m_coordinateSystems.back().push_back(vcs);
  m_scene->addItem(vcs);
}

void ClintProjection::projectScop(VizScop *vscop) {
  VizScop::VizBetaMap betaMap = vscop->vizBetaMap();

  std::vector<int> currentPileBeta, currentCoordinateSystemBeta;
  currentPileBeta.push_back(-100500);
  currentCoordinateSystemBeta.push_back(-100500);

  VizCoordinateSystem *vcs = nullptr;

  // With beta-vectors for statements, we cannot have a match that is not equality,
  // i.e. we cannot have simultaneously [1] and [1,3] as beta-vectors for statements.
  // Therefore when operating with statements, any change in beta-vector equality
  // results in a new container creation.
  // BetaMap is a map ordered by the beta-vector lexicographically. We can iterate
  // over it and stack visualized polyhedra properly.
  bool visiblePile = true;
  bool visibleCS   = true;
  // TODO: this should not work directly with betas, rather choose a VizStatement and an
  // index to select i-th beta of it.  Then compare VizStatements to each other with respect
  // to that index.  tuple<VizStatement *, index> may be reified to VizStatementOccurence
  // with ordering by beta enabled.
  for (auto betapair : betaMap) {
    std::vector<int> betaVector = betapair.first;
    VizStatement *vstmt = betapair.second;
    if (!partialBetaEquals(currentPileBeta, betaVector, 0, m_horizontalDimensionIdx + 1) &&
        visiblePile) {
      // create a new pile AND create a new CS in it
      m_coordinateSystems.emplace_back();
      createCoordinateSystem(betaVector);
      currentPileBeta.clear();
      std::copy(std::begin(betaVector), std::end(betaVector), std::back_inserter(currentPileBeta));
      currentCoordinateSystemBeta.clear();
      std::copy(std::begin(betaVector), std::end(betaVector), std::back_inserter(currentCoordinateSystemBeta));
      visiblePile = false;
      visibleCS = false;
    } else if (!partialBetaEquals(currentCoordinateSystemBeta, betaVector, m_horizontalDimensionIdx + 1, m_verticalDimensionIdx + 1) &&
               visibleCS){
      // create a new CS in the current pile
      createCoordinateSystem(betaVector);
      currentCoordinateSystemBeta.clear();
      std::copy(std::begin(betaVector), std::end(betaVector), std::back_inserter(currentCoordinateSystemBeta));
      visibleCS = false;
    }
    vcs = m_coordinateSystems.back().back();
    visibleCS = vcs->projectStatement(vstmt, betaVector) || visibleCS;
    visiblePile = visiblePile || visibleCS;
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
      if (row == 0) {
        verticalOffset = 0.0;
      }
      VizCoordinateSystem *vcs = m_coordinateSystems[col][row];
      QRectF bounding = vcs->boundingRect();
      vcs->setPos(horizontalOffset, -verticalOffset - bounding.width());
      verticalOffset += bounding.height() + coordinateSystemMargin;
      maximumWidth = std::max(maximumWidth, bounding.width());
    }
    horizontalOffset += maximumWidth + coordinateSystemMargin;
  }
}
