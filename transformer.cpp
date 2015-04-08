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
    clay_list_p list = clay_list_malloc();
    clay_array_p constArray = clay_array_malloc();
    clay_array_p paramArray = clay_array_malloc();
    clay_array_p varArray   = clay_array_malloc();
    // Clay does not need all the variables, just those used in the transformation.
    for (int i = 0, e = std::max(transformation.depth(), transformation.secondDepth()); i < e; ++i) {
      int value = 0;
      if (i + 1 == transformation.depth()) value = 1;
      else if (i + 1 == transformation.secondDepth()) value = -transformation.constantAmount();
      clay_array_add(varArray, value);
    }
    clay_list_add(list, varArray);
    clay_list_add(list, paramArray);
    clay_list_add(list, constArray);
    err = clay_shift(scop, ClayBeta(transformation.target()), transformation.depth(), list, m_options);
    clay_list_free(list);
  }
    break;
  case Transformation::Kind::IndexSetSplitting:
  {
    clay_list_p list = clay_list_malloc();
    clay_list_add(list, clay_array_clone(ClayBeta(transformation.iterators())));
    clay_list_add(list, clay_array_clone(ClayBeta(transformation.parameters())));
    clay_array_p array = clay_array_malloc();
    clay_array_add(array, transformation.constantAmount());
    clay_list_add(list, array);
    err = clay_iss(scop, ClayBeta(transformation.target()), list, nullptr, m_options);
    clay_list_free(list);
  }
    break;
  case Transformation::Kind::Grain:
    err = clay_grain(scop, ClayBeta(transformation.target()),
                    transformation.depth(),
                    transformation.constantAmount(), m_options);
    break;
  case Transformation::Kind::Reverse:
    err = clay_reverse(scop, ClayBeta(transformation.target()),
                       transformation.depth(), m_options);
    break;
  case Transformation::Kind::Interchange:
    err = clay_interchange(scop, ClayBeta(transformation.target()),
                           transformation.depth(), transformation.secondDepth(),
                           1, m_options);
    break;
  case Transformation::Kind::Tile:
    err = clay_tile(scop, ClayBeta(transformation.target()),
                    transformation.depth(), transformation.depth(),
                    transformation.constantAmount(), 0, m_options);
//    err = clay_stripmine(scop, ClayBeta(transformation.target()),
//                         transformation.depth(), transformation.constantAmount(),
//                         0, m_options);
//    err += clay_interchange(scop, ClayBeta(transformation.target()),
//                            transformation.depth(), transformation.depth() + 1,
//                            0, m_options);
    break;
  default:
    break;
  }
  CLINT_ASSERT(err == 0, "Error during Clay transformation");
  clay_beta_normalize(scop);
}

ClayBetaMapper2::ClayBetaMapper2(ClintScop *scop) {
  for (ClintStmt *stmt : scop->statements()) {
    for (ClintStmtOccurrence *occurrence : stmt->occurrences()) {
      std::vector<int> beta = occurrence->betaVector();
      m_forwardMapping.emplace(beta, beta);
      syncReverseMapping();
    }
  }
}

ClayBetaMapper2::~ClayBetaMapper2() {

}

void ClayBetaMapper2::replaceMappings(Identifier original, Identifier oldModified, Identifier newModified) {
  removeMappings(original, oldModified);
  addMappings(original, newModified);
}

void ClayBetaMapper2::addMappings(Identifier original, Identifier modified) {
  m_forwardMapping.insert(std::make_pair(original, modified));
  m_reverseMapping.insert(std::make_pair(modified, original));
}

template <typename K, typename M>
static void removeMultimapPair(std::multimap<K, M> &map, const K &key, const M &mapped) {
  typename std::multimap<K, M>::iterator beginIt, endIt, foundIt;
  std::tie(beginIt, endIt) = map.equal_range(key);
  foundIt = std::find_if(beginIt, endIt, [mapped] (const typename std::multimap<K, M>::value_type &element) {
    return element.second == mapped;
  });
  if (foundIt == endIt)
    return;
  map.erase(foundIt);
}

void ClayBetaMapper2::removeMappings(Identifier original, Identifier modified) {
  removeMultimapPair(m_forwardMapping, original, modified);
  removeMultimapPair(m_reverseMapping, modified, original);
}

void ClayBetaMapper2::apply(osl_scop_p scop, const Transformation &transformation) {
  (void) scop;

  switch (transformation.kind()) {
  case Transformation::Kind::Fuse:
  {
    Identifier target = transformation.target();
    int insertOrder = maximumAt(target);
    nextInLoop(target);

    IdentifierMultiMap updatedForwardMapping;
    std::set<Identifier> updatedCreatedMappings;
    for (auto m : m_forwardMapping) {
      Identifier identifier = m.second;
      int matchingLength = partialMatch(target, identifier);
      if (matchingLength == target.size()) {
        prevInLoop(identifier, transformation.depth() - 1);
        changeOrderAt(identifier, insertOrder + orderAt(identifier, transformation.depth()) + 1, transformation.depth());
      } else if (matchingLength == target.size() - 1 && follows(target, identifier)) {
        prevInLoop(identifier, transformation.depth() - 1);
      }

      if (m_createdMappings.find(m.second) != std::end(m_createdMappings)) {
        updatedCreatedMappings.insert(identifier);
      }
      updatedForwardMapping.emplace(m.first, identifier);
    }
    m_forwardMapping = updatedForwardMapping;
    m_createdMappings = updatedCreatedMappings;
    syncReverseMapping();
  }
    break;

  case Transformation::Kind::Split:
  {
    Identifier target = transformation.target();
    IdentifierMultiMap updatedForwardMapping;
    std::set<Identifier> updatedCreatedMappings;
    for (auto m : m_forwardMapping) {
      Identifier identifier = m.second;

      int matchingLength = partialMatch(target, identifier);
      if (matchingLength == transformation.depth() &&
          followsAt(target, identifier, transformation.depth())) {
        nextInLoop(identifier, transformation.depth() - 1);
        changeOrderAt(identifier, orderAt(identifier, transformation.depth()) - orderAt(target, transformation.depth()) - 1 /*-(orderAt(identifier) + 1)*/, transformation.depth());
      } else if (matchingLength == transformation.depth() - 1 &&
                 followsAt(target, identifier, transformation.depth() - 1)) {
        nextInLoop(identifier, transformation.depth() - 1);
      }

      if (m_createdMappings.find(m.second) != std::end(m_createdMappings)) {
        updatedCreatedMappings.insert(identifier);
      }
      updatedForwardMapping.emplace(m.first, identifier);
    }
    m_forwardMapping = updatedForwardMapping;
    m_createdMappings = updatedCreatedMappings;
    syncReverseMapping();
  }
    break;

  case Transformation::Kind::Reorder:
  {
    Identifier target = transformation.target();
    IdentifierMultiMap updatedForwardMapping;
    std::set<Identifier> updatedCreatedMappings;
//    int stmtNb = maximumAt(target) + 1;
//    CLINT_ASSERT(stmtNb == transformation.order().size(),
//                 "Order vector size mismatch");  // This assumes the normalized beta tree.

    std::vector<int> ordering = transformation.order();
    for (auto m : m_forwardMapping) {
      Identifier identifier = m.second;
      if (isPrefix(target, identifier)) {
        int oldOrder = orderAt(identifier, target.size());
        CLINT_ASSERT(oldOrder < ordering.size(),
                     "Reorder transformation vector too short");
        changeOrderAt(identifier, ordering.at(oldOrder), target.size());
      }

      if (m_createdMappings.find(m.second) != std::end(m_createdMappings)) {
        updatedCreatedMappings.insert(identifier);
      }
      updatedForwardMapping.emplace(m.first, identifier);
    }
    m_forwardMapping = updatedForwardMapping;
    m_createdMappings = updatedCreatedMappings;
    syncReverseMapping();
  }
    break;

  case Transformation::Kind::IndexSetSplitting:
  {
    Identifier target = transformation.target();
    int stmtNb = maximumAt(target);

    IdentifierMultiMap updatedForwardMapping;
    for (auto m : m_forwardMapping) {
      Identifier identifer  = m.second;
      // Insert the original statement.
      updatedForwardMapping.emplace(m.first, identifer);
      if (isPrefix(target, identifer)) {
        appendStmt(identifer, ++stmtNb);
        // Insert the iss-ed statement if needed.
        updatedForwardMapping.emplace(m.first, identifer);
        m_createdMappings.insert(identifer);
      }
    }
    m_forwardMapping = updatedForwardMapping;
    syncReverseMapping();
  }
    break;

  case Transformation::Kind::Tile:
  {
    Identifier target = transformation.target();
    IdentifierMultiMap updatedForwardMapping;
    std::set<Identifier> updatedCreatedMappings;

    for (auto m : m_forwardMapping) {
      Identifier identifier = m.second;
      if (isPrefixOrEqual(target, identifier)) {
        createLoop(identifier, transformation.depth());
      }

      if (m_createdMappings.find(m.second) != std::end(m_createdMappings)) {
        updatedCreatedMappings.insert(identifier);
      }
      updatedForwardMapping.emplace(m.first, identifier);
    }
    m_forwardMapping = updatedForwardMapping;
    m_createdMappings = updatedCreatedMappings;
    syncReverseMapping();
  }
    break;

  case Transformation::Kind::Shift:
  case Transformation::Kind::Skew:
  case Transformation::Kind::Grain:
  case Transformation::Kind::Reverse:
  case Transformation::Kind::Interchange:
    // Do not affect beta
    break;

  default:
    CLINT_UNREACHABLE;
  }
}

void ClayBetaMapper2::apply(osl_scop_p scop, const TransformationGroup &group) {
  iterativeApply(group.transformations);
}

void ClayBetaMapper2::apply(osl_scop_p scop, const TransformationSequence &sequence) {
  iterativeApply(sequence.groups);
}

void ClayBetaMapper2::dump(std::ostream &out) const {
  std::set<Identifier> uniqueKeys;
  for (auto it : m_forwardMapping) {
    uniqueKeys.insert(it.first);
  }
  for (auto key : uniqueKeys) {
    IdentifierMultiMap::const_iterator b, e;
    std::tie(b, e) = m_forwardMapping.equal_range(key);

    out << "(";
    std::copy(std::begin(key), std::end(key), std::ostream_iterator<int>(out, ","));
    out << ")  ->  {";
    for (IdentifierMultiMap::const_iterator i = b; i != e; ++i) {
      out << "(";
      std::copy(std::begin(i->second), std::end(i->second), std::ostream_iterator<int>(out, ","));
      out << "), ";
    }
    out << "}" << std::endl;
  }
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
