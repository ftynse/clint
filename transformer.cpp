#include "macros.h"
#include "transformer.h"

#include "clintscop.h"
#include "clintstmt.h"
#include "clintstmtoccurrence.h"

void ClayTransformer::apply(osl_scop_p scop, const Transformation &transformation) {
  int err = 0;
  clay_beta_normalize(scop);
  switch (transformation.kind()) {
  case Transformation::Kind::Fuse:
    err = clay_fuse(scop, ClayBeta(transformation.target()), m_options);
    break;
  case Transformation::Kind::Split:
    err = clay_split(scop, ClayBeta(transformation.target()), transformation.depth(), m_options);
    break;
  case Transformation::Kind::Shift:
  {
    // FIXME: only constant shifting is supported
    clay_list_p list = clay_list_malloc();
    clay_array_p array = clay_array_malloc();
    clay_array_add(array, transformation.constantAmount());
    clay_list_add(list, array);
    err = clay_shift(scop, ClayBeta(transformation.target()), transformation.depth(), list, m_options);
    clay_list_free(list); // List cleans inner arrays, too.
  }
    break;
  case Transformation::Kind::Reorder:
    clay_reorder(scop, ClayBeta(transformation.target()), ClayBeta(transformation.order()), m_options);
    break;
  case Transformation::Kind::Skew:
  {
    // TODO: skew transformation
//    clay_list_p list = clay_list_malloc();
//    clay_array_p constArray = clay_array_malloc();
//    clay_array_p paramArray = clay_array_malloc();
//    clay_array_p varArray   = clay_array_malloc();
//    clay_list_add(list, constArray);
//    clay_list_add(list, paramArray);
//    clay_list_add(list, varArray);
//    int depth = std::max(transformation.depth(), transformation.secondDepth());
//    err = clay_shift(scop, ClayBeta(transformation.target()), depth, list, m_options);
//    clay_list_free(list);
  }
    break;
  default:
    break;
  }
  CLINT_ASSERT(err == 0, "Error during Clay transformation");
  clay_beta_normalize(scop);
}

ClayBetaMapper::ClayBetaMapper(ClintScop *scop) {
  for (ClintStmt *stmt : scop->statements()) {
    for (ClintStmtOccurrence *occurrence : stmt->occurences()) {
      std::vector<int> beta = occurrence->betaVector();
      m_originalBeta[occurrence] = beta;
      m_originalOccurrences[beta] = occurrence;
    }
  }
}

inline bool isPrefix(const std::vector<int> &prefix, const std::vector<int> &beta, size_t length = -1ull) {
  if (length == -1ull) {
    length = prefix.size();
  }
  if (beta.size() < length + 1) {
    return false;
  }
  if (length > prefix.size()) {
    return false;
  }
  bool result = true;
  for (int i = 0; i < length; i++) {
    result = result && beta[i] == prefix[i];
  }
  return result;
}

#include <QtDebug>

void ClayBetaMapper::apply(osl_scop_p scop, const Transformation &transformation) {
//  oslListForeach(scop->statement, [this](osl_statement_p stmt) {
//    oslListForeach(stmt->scattering, [this](osl_relation_p scattering) {
//      std::vector<int> beta = betaExtract(scattering);
//      CLINT_ASSERT(m_originalOccurrences.find(beta) != std::end(m_originalOccurrences),
//                   "A mismatching scop supplied");
//    });
//  });

  switch (transformation.kind()) {
  case Transformation::Kind::Fuse:
  {
    CLINT_ASSERT(transformation.target().size() >= 1, "Cannot fuse root");
    int idx = 0;
    int nextValue = INT_MAX;
    for (auto it : m_originalOccurrences) {
      const std::vector<int> &originalBeta = it.first;
      const std::vector<int> &target = transformation.target();
      if (isPrefix(target, originalBeta)) {
        idx = std::max(idx, originalBeta[target.size()]);
      }
      if (isPrefix(target, originalBeta, target.size() - 1) &&
          originalBeta[target.size() - 1] > target.back()) {
        nextValue = std::min(nextValue, originalBeta[target.size() - 1]);
      }
    }
    CLINT_ASSERT(nextValue != INT_MAX, "Nothing to fuse with");

    for (auto it : m_originalOccurrences) {
      const std::vector<int> &originalBeta = it.first;
      const std::vector<int> &target = transformation.target();
      std::vector<int> updatedBeta(originalBeta);
      if (target.size() < originalBeta.size() &&
          isPrefix(target, originalBeta, target.size() - 1)){
        if (originalBeta[target.size() - 1] == nextValue) {
          updatedBeta[target.size() - 1]--;
          updatedBeta[target.size()] = ++idx;
        } else if (originalBeta[target.size() - 1] > nextValue) {
          updatedBeta[target.size() - 1]--;
        }
      }
      m_updatedBeta[it.second] = updatedBeta;
    }
  }
    break;
  case Transformation::Kind::Split:
    CLINT_ASSERT(transformation.depth() >= 1, "Cannot split at depth 0");
    CLINT_ASSERT(transformation.target().size() > transformation.depth(), "Split depth overflow");
    for (auto it : m_originalOccurrences) {
      const std::vector<int> &originalBeta = it.first;
      const std::vector<int> &target(transformation.target());
      std::vector<int> updatedBeta(originalBeta);
      if (isPrefix(target, originalBeta, transformation.depth()) &&
          originalBeta[transformation.depth()] > target[transformation.depth()]) {
        CLINT_ASSERT(updatedBeta.size() >= 1, "Cannot split root");
        updatedBeta[transformation.depth() - 1]++;
        updatedBeta[transformation.depth()] -= (target.back() + 1);
      } else if (isPrefix(target, originalBeta, transformation.depth() - 1) &&
                 originalBeta[transformation.depth() - 1] > target[transformation.depth() - 1]) {
        updatedBeta[transformation.depth() - 1]++;
      }
      m_updatedBeta[it.second] = updatedBeta;
    }
    break;
  case Transformation::Kind::Reorder:
  {
    const std::vector<int> &order = transformation.order();
    for (auto it : m_originalOccurrences) {
      const std::vector<int> &originalBeta = it.first;
      const std::vector<int> &target = transformation.target();
      std::vector<int> updatedBeta(originalBeta);
      if (isPrefix(target, originalBeta)) {
        size_t idx = target.size();
        CLINT_ASSERT(order.size() > updatedBeta[idx], "Reorder vector is too short");
        updatedBeta[idx] = order[updatedBeta[idx]];
      }
      m_updatedBeta[it.second] = updatedBeta;
    }
  }
    break;
  case Transformation::Kind::Shift:
  case Transformation::Kind::Skew:
    // Do not affect beta
    break;
  }

  m_updatedOccurrences.clear();
  for (auto it : m_updatedBeta) {
    m_updatedOccurrences[it.second] = it.first;
  }
}

void ClayBetaMapper::apply(osl_scop_p scop, const TransformationGroup &group) {
  bool first = true;
  decltype(m_originalBeta) originalBetaInit;
  decltype(m_originalOccurrences) originalOccurrencesInit;
  for (const Transformation &t : group.transformations) {
    if (first) {
      first = false;
      originalBetaInit = m_originalBeta;
      originalOccurrencesInit = m_originalOccurrences;
    } else {
      m_originalBeta = m_updatedBeta;
      m_originalOccurrences = m_updatedOccurrences;
      m_updatedBeta.clear();
      m_originalBeta.clear();
    }
    apply(scop, t);
  }
  m_originalBeta = originalBetaInit;
  m_originalOccurrences = originalOccurrencesInit;
}

std::vector<int> ClayTransformer::transformedBeta(const std::vector<int> &beta, const Transformation &transformation) {
  std::vector<int> tBeta(beta);
  switch (transformation.kind()) {
  case Transformation::Kind::Fuse:
    if (beta > transformation.target()) {
      tBeta[transformation.depth()] -= 1;
    }
    break;
  default:
    break;
  }
  return std::move(tBeta);
}

std::vector<int> ClayTransformer::originalBeta(const std::vector<int> &beta, const Transformation &transformation) {
  std::vector<int> tBeta(beta);
  switch (transformation.kind()) {
  case Transformation::Kind::Fuse:
    if (beta > transformation.target()) {
      tBeta[transformation.depth()] += 1;
    }
    break;
  default:
    break;
  }
  return std::move(tBeta);
}
