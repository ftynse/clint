#include "oslutils.h"
#include "clintstmt.h"
#include "clintstmtoccurrence.h"

#include <functional>

ClintStmtOccurrence::ClintStmtOccurrence(osl_statement_p stmt, const std::vector<int> &betaVector,
                                     ClintStmt *parent) :
  QObject(parent), m_oslStatement(stmt), m_statement(parent) {
  resetOccurrence(stmt, betaVector);
}

/// The split occurrence will hot have osl_statement set up by default.  Use resetOccurrence to initialize it.
ClintStmtOccurrence *ClintStmtOccurrence::split(const std::vector<int> &betaVector) {
  ClintStmtOccurrence *occurrence = new ClintStmtOccurrence(nullptr, betaVector, m_statement);
  occurrence->m_tilingDimensions = m_tilingDimensions;
  occurrence->m_tileSizes = m_tileSizes;
  return occurrence;
}

void ClintStmtOccurrence::resetOccurrence(osl_statement_p stmt, const std::vector<int> &betaVector) {
  bool differentBeta = (m_betaVector == betaVector);
  bool differentPoints = false;
  m_betaVector = betaVector;
  m_oslStatement = stmt;

  if (stmt == nullptr) {
    if (m_oslScattering != nullptr)
      emit pointsChanged();
    if (differentBeta)
      emit betaChanged();
    return;
  }

  osl_relation_p oslScattering = nullptr;
  oslListForeach(stmt->scattering, [&oslScattering,&betaVector](osl_relation_p scattering) {
    if (betaExtract(scattering) == betaVector) {
      CLINT_ASSERT(oslScattering == nullptr,
                   "Duplicate beta-vector found");
      oslScattering = scattering;
    }
  });
  CLINT_ASSERT(oslScattering != nullptr,
               "Trying to create an occurrence for the inexistent beta-vector");

  differentPoints = !osl_relation_equal(oslScattering, m_oslScattering);
  m_oslScattering = oslScattering;

  if (differentPoints) {
    emit pointsChanged();
  }
  if (differentBeta) {
    emit betaChanged();
  }
}

void ClintStmtOccurrence::resetBetaVector(const std::vector<int> &betaVector) {
  bool differentBeta = (m_betaVector == betaVector);
  m_betaVector = betaVector;

  if (differentBeta)
    emit betaChanged();
}

bool operator < (const ClintStmtOccurrence &lhs, const ClintStmtOccurrence &rhs) {
  return lhs.m_betaVector < rhs.m_betaVector;
}

bool operator ==(const ClintStmtOccurrence &lhs, const ClintStmtOccurrence &rhs) {
  return lhs.m_betaVector == rhs.m_betaVector;
}

int ClintStmtOccurrence::ignoreTilingDim(int dim) const {
  // Ignore projections on tiled dimensions.
  for (int tilingDim : m_tilingDimensions) {
    if (tilingDim > dim) {
      return dim;
    }
    dim++;
  }
  return dim;
}

std::vector<std::vector<int>> ClintStmtOccurrence::projectOn(int horizontalDimIdx, int verticalDimIdx) const {
  CLINT_ASSERT(m_oslStatement != nullptr && m_oslScattering != nullptr,
               "Trying to project a non-initialized statement");

  // Transform iterator (alpha only) indices to enumerator (beta-alpha-beta) indices
  // betaDims are found properly iff the dimensionalityChecker assertion holds.
  int horizontalScatDimIdx    = 1 + 2 * horizontalDimIdx;
  int verticalScatDimIdx      = 1 + 2 * verticalDimIdx;

  horizontalScatDimIdx  = ignoreTilingDim(horizontalScatDimIdx);
  verticalScatDimIdx    = ignoreTilingDim(verticalScatDimIdx);

  // Checking if all the relations for the same beta have the same structure.
  // AZ: Not sure if it is theoretically possible: statements with the same beta-vector
  // should normally be in the same part of the domain union and therefore have same
  // number of scattering variables.  If the following assertion ever fails, check the
  // theoretical statement and if it does not hold, perform enumeration separately for
  // each union part.
  // AZ: this holds for SCoPs generated by Clan.
  // TODO: piggyback on oslUtils and Enumerator to pass around the mapping from original iterators
  // to the scattering (indices in applied matrix) since different parts of the domain and scattering
  // relation unions may have different number of input/output dimensions for the codes
  // originating from outside Clan/Clay toolset.
  // AZ: is this even possible without extra information?  Two problematic cases,
  // 1. a1 = i + j, a2 = i - j; no clear mapping between a and i,j
  // 2. stripmine a1 >/< f(i), a2 = i; two a correspond to i in the same time.
  auto dimensionalityCheckerFunction = [](osl_relation_p rel, int output_dims,
                                          int input_dims, int parameters) {
    CLINT_ASSERT(rel->nb_output_dims == output_dims,
                 "Dimensionality mismatch, proper polyhedron construction impossible");
    CLINT_ASSERT(rel->nb_input_dims == input_dims,
                 "Dimensionality mismatch, proper polyhedron construction impossible");
    CLINT_ASSERT(rel->nb_parameters == parameters,
                 "Dimensionality mismatch, proper polyhedron construction impossible");
  };
  oslListForeach(m_oslStatement->domain, dimensionalityCheckerFunction, m_oslStatement->domain->nb_output_dims,
                 m_oslStatement->domain->nb_input_dims, m_oslStatement->domain->nb_parameters);
  CLINT_ASSERT(m_oslStatement->domain->nb_output_dims == m_oslScattering->nb_input_dims,
               "Scattering is not compatible with the domain");

  // Get original and scattered iterator values depending on the axis displayed.
  bool projectHorizontal = (horizontalDimIdx != -2) && (dimensionality() > horizontalDimIdx); // FIXME: -2 is in VizProperties::NO_DIMENSION, but should be in a different class since it has nothing to do with viz
  bool projectVertical   = (verticalDimIdx != -2) && (dimensionality() > verticalDimIdx);
  CLINT_ASSERT(!(projectHorizontal ^ (horizontalScatDimIdx >= 0 && horizontalScatDimIdx < m_oslScattering->nb_output_dims)),
               "Trying to project to the horizontal dimension that is not present in scattering");
  CLINT_ASSERT(!(projectVertical ^ (verticalScatDimIdx >= 0 && verticalScatDimIdx < m_oslScattering->nb_output_dims)),
               "Trying to project to the vertical dimension that is not present in scattering");

  std::vector<osl_relation_p> scatterings { m_oslScattering };
  osl_relation_p applied = oslApplyScattering(oslListToVector(m_oslStatement->domain),
                                              scatterings);
  osl_relation_p ready = oslRelationsWithContext(applied, m_statement->scop()->fixedContext());

  std::vector<int> allDimensions;
  allDimensions.reserve(m_oslScattering->nb_output_dims + 2);
  if (projectHorizontal) {
    allDimensions.push_back(horizontalScatDimIdx);
  }
  if (projectVertical) {
    allDimensions.push_back(verticalScatDimIdx);
  }
  for (int i = 0, e = m_oslScattering->nb_input_dims; i < e; ++i) {
    allDimensions.push_back(m_oslScattering->nb_output_dims + i);
  }
  std::vector<std::vector<int>> points = program()->enumerator()->enumerate(ready, allDimensions);
  computeMinMax(points, horizontalDimIdx, verticalDimIdx);

  return std::move(points);
}

std::pair<std::vector<int>, std::pair<int, int>> ClintStmtOccurrence::parseProjectedPoint(std::vector<int> point,
                                                                                          int horizontalDimIdx,
                                                                                          int verticalDimIdx) const {
  std::pair<int, int> scatteredCoordinates {INT_MAX, INT_MAX}; // FIXME: INT_MAX is VizPoint::NO_COORD

  if (visibleDimensionality() >= horizontalDimIdx && horizontalDimIdx != -2) {
    CLINT_ASSERT(point.size() > 0, "Not enough dimensions");
    scatteredCoordinates.first = point[0];
    point.erase(std::begin(point));
  }
  if (visibleDimensionality() >= verticalDimIdx && verticalDimIdx != -2) {
    CLINT_ASSERT(point.size() > 0, "Not enough dimensions");
    scatteredCoordinates.second = point[0];
    point.erase(std::begin(point));
  }

  return std::make_pair(point, scatteredCoordinates);
}


void ClintStmtOccurrence::computeMinMax(const std::vector<std::vector<int>> &points,
                                        int horizontalDimIdx, int verticalDimIdx) const {
  int horizontalMin, horizontalMax, verticalMin = 0, verticalMax = 0;
  std::vector<int> originalMins, originalMaxs;
  if (horizontalDimIdx == -2) { // FIXME: -2 is in VizProperties::NO_DIMENSION
    return;
  }
  if (points.size() == 0) {
    m_cachedDimMins[horizontalDimIdx] = 0;
    m_cachedDimMaxs[horizontalDimIdx] = 0;
    m_cachedDimMins[verticalDimIdx] = 0;
    m_cachedDimMaxs[verticalDimIdx] = 0;
    return;
  }

  std::vector<int> originalCoordiantes;
  std::pair<int, int> scatteredCoordinates;
  std::tie(originalCoordiantes, scatteredCoordinates) = parseProjectedPoint(points.front(), horizontalDimIdx, verticalDimIdx);
  bool computeHorizontal = scatteredCoordinates.first != INT_MAX && horizontalDimIdx != -2; // FIXME: INT_MAX is VizPoint::NO_COORD
  bool computeVertical = scatteredCoordinates.second != INT_MAX && verticalDimIdx != -2;
  if (computeHorizontal) {
    horizontalMin = INT_MAX;
    horizontalMax = INT_MIN;
  }
  if (computeVertical) {
    verticalMin = INT_MAX;
    verticalMax = INT_MIN;
  }
  originalMins.resize(originalCoordiantes.size(), INT_MAX);
  originalMaxs.resize(originalCoordiantes.size(), INT_MIN);

  // Compute min and max values for projected iterators of the current coordinate system.
  for (const std::vector<int> &point : points) {
    std::tie(originalCoordiantes, scatteredCoordinates) =
        parseProjectedPoint(point, horizontalDimIdx, verticalDimIdx);
    if (computeHorizontal) {
      horizontalMin = std::min(horizontalMin, scatteredCoordinates.first);
      horizontalMax = std::max(horizontalMax, scatteredCoordinates.first);
    }
    if (computeVertical) {
      verticalMin = std::min(verticalMin, scatteredCoordinates.second);
      verticalMax = std::max(verticalMax, scatteredCoordinates.second);
    }
    for (size_t i = 0, e = originalMins.size(); i < e; ++i) {
      originalMins[i] = std::min(originalCoordiantes[i], originalMins[i]);
      originalMaxs[i] = std::max(originalCoordiantes[i], originalMaxs[i]);
    }
  }
  CLINT_ASSERT(horizontalMin != INT_MIN, "Could not compute horizontal minimum");
  CLINT_ASSERT(horizontalMax != INT_MAX, "Could not compute horizontal maximum");
  CLINT_ASSERT(verticalMin != INT_MIN, "Could not compute vertical minimum");
  CLINT_ASSERT(verticalMax != INT_MAX, "Could not compute vertical maximum");

  if (computeHorizontal) {
    m_cachedDimMins[horizontalDimIdx] = horizontalMin;
    m_cachedDimMaxs[horizontalDimIdx] = horizontalMax;
  }
  if (computeVertical) {
    m_cachedDimMins[verticalDimIdx] = verticalMin;
    m_cachedDimMaxs[verticalDimIdx] = verticalMax;
  }
  for (size_t i = 0, e = originalMins.size(); i < e; ++i) {
    m_cachedOrigDimMins[i] = originalMins[i];
    m_cachedOrigDimMaxs[i] = originalMaxs[i];
  }
}

std::vector<int> ClintStmtOccurrence::makeBoundlikeForm(Bound bound, int dimIdx, int constValue,
                                                        int constantBoundaryPart,
                                                        const std::vector<int> &parameters,
                                                        const std::vector<int> &parameterValues) {
  bool isConstantForm = std::all_of(std::begin(parameters), std::end(parameters),
                                    std::bind(std::equal_to<int>(), std::placeholders::_1, 0));
  // Substitute parameters with current values.
  // Constant will differ depending on the target form:
  // for the constant form, all parameters are replaced with respective values;
  // for the parametric form, adjust with the current constant part of the boundary.
  int parameterSubstValue = 0;
  for (size_t i = 0; i < parameters.size(); i++) {
    parameterSubstValue += parameters[i] * parameterValues[i];
  }
  parameterSubstValue += constantBoundaryPart;
  if (bound == Bound::LOWER && isConstantForm) {
    constValue = constValue - 1;
  } else if (bound == Bound::UPPER && isConstantForm) {
    constValue = -constValue - 1;
  } else if (bound == Bound::LOWER && !isConstantForm) {
    constValue = -constantBoundaryPart + (constValue + parameterSubstValue - 1);
  } else if (bound == Bound::UPPER && !isConstantForm) {
    constValue = -constantBoundaryPart + (parameterSubstValue - constValue - 1);
  } else {
    CLINT_UNREACHABLE;
  }

  std::vector<int> form;
  form.push_back(1); // Inequality
  for (int i = 0; i < m_oslStatement->domain->nb_output_dims; i++) {
    form.push_back(i == dimIdx ? (bound == Bound::LOWER ? -1 : 1) : 0);
  }
  for (int p : parameters) {
    form.push_back(-p);
  }
  form.push_back(constValue);
  return std::move(form);
}

static bool isVariableBoundary(int row, osl_relation_p scattering, int dimIdx) {
  bool variable = false;
  for (int i = 0; i < scattering->nb_input_dims + scattering->nb_output_dims + scattering->nb_local_dims; i++) {
    if (i + 1 == dimIdx)
      continue;
    if (!osl_int_zero(scattering->precision, scattering->m[row][1 + i]))
      variable = true;
  }

  return variable;
}

static bool isParametricBoundary(int row, osl_relation_p scattering) {
  int firstParameterDim = scattering->nb_input_dims +
      scattering->nb_output_dims +
      scattering->nb_local_dims + 1;
  for (int i = 0; i < scattering->nb_parameters; i++) {
    if (!osl_int_zero(scattering->precision, scattering->m[row][firstParameterDim + i]))
      return true;
  }
  return false;
}

std::vector<int> ClintStmtOccurrence::findBoundlikeForm(Bound bound, int dimIdx, int constValue) {
  std::vector<int> zeroParameters(m_oslStatement->domain->nb_parameters, 0);
  std::vector<int> parameterValues = scop()->parameterValues();

  int oslDimIdx = 1 + dimIdx * 2 + 1;
  osl_relation_p scheduledDomain = ISLEnumerator::scheduledDomain(m_oslStatement->domain, m_oslScattering);
  int lowerRow = oslRelationDimUpperBound(scheduledDomain, oslDimIdx);
  int upperRow = oslRelationDimLowerBound(scheduledDomain, oslDimIdx);

  // Check if the request boundary exists and is unique.
  // If failed, try the opposite boundary.  If the opposite does not
  // exist or is not unique, use constant form.
  if (lowerRow < 0 && upperRow < 0) {
    return makeBoundlikeForm(bound, dimIdx, constValue, 0, zeroParameters, parameterValues);
  } else if (lowerRow < 0 && bound == Bound::LOWER) {
    bound = Bound::UPPER;
  } else if (upperRow < 0 && bound == Bound::UPPER) {
    bound = Bound::LOWER;
  }

  // Check if the current boundary (either request or opposite) is
  // variable.  If it is, try the opposite unless the opposite does
  // not exist.  If both are variable or one is variable and another
  // does not exit, use constant form.
  bool upperVariable = upperRow >= 0 ? isVariableBoundary(upperRow, scheduledDomain, oslDimIdx) : true;
  bool lowerVariable = lowerRow >= 0 ? isVariableBoundary(lowerRow, scheduledDomain, oslDimIdx) : true;
  if (upperVariable && lowerVariable) {
    return makeBoundlikeForm(bound, dimIdx, constValue, 0, zeroParameters, parameterValues);
  }
  if (upperVariable && lowerRow >= 0) {
    bound = Bound::LOWER;
  } else if (lowerVariable && upperRow >= 0) {
    bound = Bound::UPPER;
  }
  int row = bound == Bound::UPPER ? upperRow : lowerRow;

  int firstParameterIdx = 1 + scheduledDomain->nb_input_dims + scheduledDomain->nb_output_dims + scheduledDomain->nb_local_dims;
  std::vector<int> parameters;
  parameters.reserve(scheduledDomain->nb_parameters);
  for (int i = 0; i < scheduledDomain->nb_parameters; i++) {
    int p = osl_int_get_si(scheduledDomain->precision, scheduledDomain->m[row][firstParameterIdx + i]);
    parameters.push_back(p);
  }

  // Make parametric form is the boundary itself is parametric.
  // Make constant form otherwise.
  if (isParametricBoundary(row, scheduledDomain)) {
    int boundaryConstantPart = osl_int_get_si(scheduledDomain->precision,
                                              scheduledDomain->m[row][scheduledDomain->nb_columns - 1]);
    return makeBoundlikeForm(bound, dimIdx, constValue,
                             boundaryConstantPart,
                             parameters, parameterValues);
  } else {
    return makeBoundlikeForm(bound, dimIdx, constValue,
                             0, zeroParameters, parameterValues);
  }
}

int ClintStmtOccurrence::minimumValue(int dimIdx) const {
  if (dimIdx >= dimensionality() || dimIdx < 0)
    return 0;
  if (m_cachedDimMins.count(dimIdx) == 0) {
    projectOn(dimIdx, -2); // FIXME: -2 is VizProperties::NoDimension
  }
  CLINT_ASSERT(m_cachedDimMins.count(dimIdx) == 1,
               "min cache failure");
  return m_cachedDimMins[dimIdx];
}

int ClintStmtOccurrence::maximumValue(int dimIdx) const {
  if (dimIdx >= dimensionality() || dimIdx < 0)
    return 0;
  if (m_cachedDimMaxs.count(dimIdx) == 0) {
    projectOn(dimIdx, -2); // FIXME: -2 is VizProperties::NoDimension
  }
  CLINT_ASSERT(m_cachedDimMaxs.count(dimIdx) == 1,
               "max cache failure");
  return m_cachedDimMaxs[dimIdx];
}

int ClintStmtOccurrence::minimumOrigValue(int dimIdx) const {
  if (dimIdx >= inputDimensionality() || dimIdx < 0)
    return 0;
  if (m_cachedOrigDimMins.count(dimIdx) == 0) {
    projectOn(-2, -2);
  }
  CLINT_ASSERT(m_cachedOrigDimMins.count(dimIdx) == 1,
               "orig min cache failure");
  return m_cachedOrigDimMins[dimIdx];
}

int ClintStmtOccurrence::maximumOrigValue(int dimIdx) const {
  if (dimIdx >= inputDimensionality() || dimIdx < 0)
    return 0;
  if (m_cachedOrigDimMaxs.count(dimIdx) == 0) {
    projectOn(-2, -2);
  }
  CLINT_ASSERT(m_cachedOrigDimMaxs.count(dimIdx) == 1,
               "orig min cache failure");
  return m_cachedOrigDimMaxs[dimIdx];
}

std::vector<int> ClintStmtOccurrence::untiledBetaVector() const {
  std::vector<int> beta(m_betaVector);
  // m_tilingDimensions is an ordered set, start from the end to remove higher
  // indices from beta-vector first.  Thus lower indices will remain the same.
  for (auto it = m_tilingDimensions.rbegin(), eit = m_tilingDimensions.rend();
       it != eit; it++) {
    if ((*it) % 2 == 0) {
      int index = (*it) / 2;
      beta.erase(beta.begin() + index);
    }
  }
  return std::move(beta);
}

void ClintStmtOccurrence::tile(int depth, unsigned tileSize) {
  CLINT_ASSERT(tileSize != 0,
               "Cannot tile by 0 elements");

  // For 0-based indexing (depth is 1-based).
  depth -= 1;

  std::set<int> tilingDimensions;
  std::unordered_map<int, unsigned> tileSizes;
  for (int dim : m_tilingDimensions) {
    if (dim >= 2 * depth + 1) {
      tilingDimensions.insert(dim + 2);
      if (m_tileSizes.count(dim))
        tileSizes[dim + 2] = m_tileSizes[dim];
    } else {
      tilingDimensions.insert(dim);
      if (m_tileSizes.count(dim))
        tileSizes[dim] = m_tileSizes[dim];
    }
  }
  tilingDimensions.insert(2 * depth + 2);
  tilingDimensions.insert(2 * depth + 1);
  tileSizes[2 * depth + 1] = tileSize;
  m_tilingDimensions = tilingDimensions;
  m_tileSizes = tileSizes;
}

void ClintStmtOccurrence::untile(int depth) {
  // For 0-based indexing (depth is 1-based).
  depth -= 1;

  std::set<int> tilingDimensions;
  std::unordered_map<int, unsigned> tileSizes;
  m_tilingDimensions.erase(2 * depth + 2);
  m_tilingDimensions.erase(2 * depth + 1);
  m_tileSizes.erase(2 * depth + 1);
  for (int dim : m_tilingDimensions) {
    if (dim > 2 * depth + 2) {
      tilingDimensions.insert(dim - 2);
      if (m_tileSizes.count(dim))
        tileSizes[dim - 2] = m_tileSizes[dim];
    } else {
      tilingDimensions.insert(dim);
      if (m_tileSizes.count(dim))
        tileSizes[dim] = m_tileSizes[dim];
    }
  }
  m_tilingDimensions = tilingDimensions;
  m_tileSizes = tileSizes;
}
