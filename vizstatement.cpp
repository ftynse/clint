#include "enumerator.h"
#include "macros.h"
#include "oslutils.h"
#include "vizstatement.h"

#include <utility>

VizStatement::VizStatement(osl_statement_p stmt, VizScop *parent) :
  QObject(parent), m_scop(parent), m_statement(stmt) {

}

std::vector<std::vector<int>> VizStatement::projectOn(int horizontalDimIdx,
                                                      int verticalDimIdx,
                                                      const std::vector<int> &betaVector) const {
  // Transform iterator (alpha only) indices to enumerator (beta-alpha-beta) indices
  // betaDims are found properly iff the dimensionalityChecker assertion holds.
  int horizontalDimOsl    = 1 + 2 * horizontalDimIdx;
  int verticalDimOsl      = 1 + 2 * verticalDimIdx;
  int horizontalBetaDim   = 1 + 2 * m_statement->domain->nb_output_dims + horizontalDimIdx;
  int verticalBetaDim     = 1 + 2 * m_statement->domain->nb_output_dims + verticalDimIdx;

  // Get original and scattered iterator values depending on the axis displayed.
  std::vector<int> visibleDimensions;
  bool projectHorizontal = (betaVector.size() - 1) > horizontalDimIdx;
  bool projectVertical   = (betaVector.size() - 1) > verticalDimIdx;
  if (!projectHorizontal && !projectVertical) {
    // This is just a point, no actual enumeration needed.
    // All the dimensions being projected out, the result of enumeration is a single zero-dimensional point.
  } else if (projectHorizontal && !projectVertical) {
    visibleDimensions.push_back(horizontalDimOsl);
    visibleDimensions.push_back(horizontalBetaDim);
  } else if (!projectHorizontal && projectVertical) {
    visibleDimensions.push_back(verticalDimOsl);
    visibleDimensions.push_back(verticalBetaDim);
  } else {
    visibleDimensions.push_back(horizontalDimOsl);
    visibleDimensions.push_back(verticalDimOsl);
    visibleDimensions.push_back(horizontalBetaDim);
    visibleDimensions.push_back(verticalBetaDim);
  }

  osl_relation_p applied = oslApplyScattering(m_statement, betaVector);
  osl_relation_p ready = oslRelationsWithContext(applied, m_scop->fixedContext());

  std::vector<std::vector<int>> points =
      program()->enumerator()->enumerate(ready, std::move(visibleDimensions));

  return std::move(points);
}
