#include "macros.h"
#include "transformer.h"

#include "clintscop.h"
#include "clintstmt.h"
#include "clintstmtoccurrence.h"

#include <exception>

#include <clay/errors.h>

#include <chlore/chlore.h>

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
    // FIXME: this is a workaround for the old definition of variable shift, absent in cgo-clay
    if (transformation.iterators().size() != 0) {
      CLINT_WARNING(true, "Variable shift emulated through skew");
      const std::vector<int> &iterators = transformation.iterators();
      for (size_t i = 0; i < iterators.size(); i++) {
        if (iterators[i] != 0) {
          clay_skew(scop, ClayBeta(transformation.target()), transformation.depth(),
                    i + 1, iterators[i], m_options);
        }
      }
    }

    err = clay_shift(scop, ClayBeta(transformation.target()), transformation.depth(),
                     ClayBeta(transformation.parameters()), transformation.constantAmount(),
                     m_options);
  }
    break;
  case Transformation::Kind::Reorder:
    clay_reorder(scop, ClayBeta(transformation.target()), ClayBeta(transformation.order()), m_options);
    break;
  case Transformation::Kind::Skew:
  {
    err = clay_skew(scop, ClayBeta(transformation.target()), transformation.depth(),
                    transformation.secondDepth(), transformation.constantAmount(), m_options);
  }
    break;
  case Transformation::Kind::Reshape:
  {
    err = clay_reshape(scop, ClayBeta(transformation.target()), transformation.depth(),
                       transformation.secondDepth(), transformation.constantAmount(), m_options);
    break;
  }
  case Transformation::Kind::IndexSetSplitting:
  {
    clay_list_p list = clay_list_malloc();
    clay_list_add(list, clay_array_clone(ClayBeta(transformation.iterators())));
    clay_list_add(list, clay_array_malloc()); // Space for input dimensions.
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
                    transformation.constantAmount(), m_options);
    break;
  case Transformation::Kind::Linearize:
    err = clay_linearize(scop, ClayBeta(transformation.target()),
                         transformation.depth(), m_options);
    break;
  case Transformation::Kind::Densify:
    err = clay_densify(scop, ClayBeta(transformation.target()),
                       transformation.depth(), m_options);
    break;
  case Transformation::Kind::Collapse:
    err = clay_collapse(scop, ClayBeta(transformation.target()), m_options);
    break;
  case Transformation::Kind::Embed:
    err = clay_embed(scop, ClayBeta(transformation.target()), m_options);
    break;
  case Transformation::Kind::Unembed:
    err = clay_unembed(scop, ClayBeta(transformation.target()), m_options);
    break;
  default:
    break;
  }
  if (err != CLAY_SUCCESS) {
    throw std::logic_error(std::string(clay_error_message_text(err)));
  }
  clay_beta_normalize(scop);
}

boost::optional<Transformation> ClayTransformer::guessInverseTransformation(osl_scop_p scop, const Transformation &transformation) {
  switch (transformation.kind()) {
  case Transformation::Kind::Densify:
  {
    int grain = chlore_extract_grain(scop, ClayBeta(transformation.target()), transformation.depth());
    if (grain > 1)
      return Transformation::grain(transformation.target(), transformation.depth(), grain);
    else
      return boost::none;
  }

  case Transformation::Kind::Linearize:
  {
    int size = chlore_extract_stripmine_size(scop, ClayBeta(transformation.target()), transformation.depth());
    if (size > 1)
      return Transformation::tile(transformation.target(), transformation.depth(), size);
    else
      return boost::none;
  }

  case Transformation::Kind::Unembed:
    if (chlore_extract_embed(scop, ClayBeta(transformation.target())))
      return Transformation::embed(transformation.target());
    else
      return boost::none;

  case Transformation::Kind::Collapse:
  {
    clay_list_p condition = chlore_extract_iss_condition(scop, ClayBeta(transformation.target()));
    if (condition) {
      std::vector<std::vector<int>> list;
      list.resize(4);
      for (int i = 0; i < 4; i++) {
        list[i] = betaFromClay(condition->data[i]);
      }
      return Transformation::rawIss(transformation.target(), list);
    } else {
      return boost::none;
    }
  }

  case Transformation::Kind::Fuse:
  {
    clay_array_p beta_max = clay_beta_max(scop->statement, ClayBeta(transformation.target()));
    int maximum = beta_max->data[transformation.target().size()];
    clay_array_free(beta_max);
    std::vector<int> beta = transformation.target();
    beta.push_back(maximum);
    return Transformation::splitAfter(beta);
  }

  case Transformation::Kind::Split:
  {
    std::vector<int> betaPrefix = transformation.target();
    betaPrefix.resize(betaPrefix.size() - 1);
    return Transformation::fuseNext(betaPrefix);
  }

  case Transformation::Kind::Reorder:
  {
    std::vector<int> order = transformation.order();
    std::vector<int> new_order;
    new_order.resize(order.size());
    for (size_t i = 0; i < order.size(); ++i) {
      new_order[order[i]] = i;
    }
    return Transformation::rawReorder(transformation.target(), new_order);
  }

  case Transformation::Kind::Tile:
  {
    return Transformation::linearize(transformation.target(), transformation.depth());
  }

  case Transformation::Kind::IndexSetSplitting:
  {
    return Transformation::collapse(transformation.target());
  }

  default:
    throw std::invalid_argument("Cannot guess parameters for this transformation");
  }
}

ClayBetaMapper::ClayBetaMapper(ClintScop *scop) {
  for (ClintStmt *stmt : scop->statements()) {
    for (ClintStmtOccurrence *occurrence : stmt->occurrences()) {
      std::vector<int> beta = occurrence->betaVector();
      m_forwardMapping.emplace(beta, beta);
      syncReverseMapping();
    }
  }
}

ClayBetaMapper::~ClayBetaMapper() {
}

void ClayBetaMapper::reset() {
  std::set<Identifier> keys;
  for (auto it : m_forwardMapping) {
    keys.insert(it.first);
  }
  m_forwardMapping.clear();
  for (auto it : keys) {
    m_forwardMapping.insert(std::make_pair(it, it));
  }
  syncReverseMapping();
}

void ClayBetaMapper::replaceMappings(Identifier original, Identifier oldModified, Identifier newModified) {
  removeMappings(original, oldModified);
  addMappings(original, newModified);
}

void ClayBetaMapper::addMappings(Identifier original, Identifier modified) {
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

void ClayBetaMapper::removeMappings(Identifier original, Identifier modified) {
  removeMultimapPair(m_forwardMapping, original, modified);
  removeMultimapPair(m_reverseMapping, modified, original);
}

void ClayBetaMapper::apply(osl_scop_p scop, const Transformation &transformation) {
  (void) scop;

  switch (transformation.kind()) {
  case Transformation::Kind::Fuse:
  {
    Identifier target = transformation.target();
    int insertOrder = maximumAt(target);
    BetaUtility::nextInLoop(target);

    IdentifierMultiMap updatedForwardMapping;
    std::set<Identifier> updatedCreatedMappings;
    for (auto m : m_forwardMapping) {
      Identifier identifier = m.second;
      int matchingLength = BetaUtility::partialMatch(target, identifier);
      if (matchingLength == target.size()) {
        BetaUtility::prevInLoop(identifier, transformation.depth() - 1);
        BetaUtility::changeOrderAt(identifier, insertOrder + BetaUtility::orderAt(identifier, transformation.depth()) + 1, transformation.depth());
      } else if (matchingLength == target.size() - 1 && BetaUtility::follows(target, identifier)) {
        BetaUtility::prevInLoop(identifier, transformation.depth() - 1);
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

      int matchingLength = BetaUtility::partialMatch(target, identifier);
      if (matchingLength == transformation.depth() &&
          BetaUtility::followsAt(target, identifier, transformation.depth())) {
        BetaUtility::nextInLoop(identifier, transformation.depth() - 1);
        BetaUtility::changeOrderAt(identifier, BetaUtility::orderAt(identifier, transformation.depth()) - BetaUtility::orderAt(target, transformation.depth()) - 1 /*-(orderAt(identifier) + 1)*/, transformation.depth());
      } else if (matchingLength == transformation.depth() - 1 &&
                 BetaUtility::followsAt(target, identifier, transformation.depth() - 1)) {
        BetaUtility::nextInLoop(identifier, transformation.depth() - 1);
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
      if (BetaUtility::isPrefix(target, identifier)) {
        int oldOrder = BetaUtility::orderAt(identifier, target.size());
        CLINT_ASSERT(oldOrder < ordering.size(),
                     "Reorder transformation vector too short");
        BetaUtility::changeOrderAt(identifier, ordering.at(oldOrder), target.size());
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
      if (BetaUtility::isPrefix(target, identifer)) {
        BetaUtility::appendStmt(identifer, ++stmtNb);
        // Insert the iss-ed statement if needed.
        updatedForwardMapping.emplace(m.first, identifer);
        m_createdMappings.insert(identifer);
      }
    }
    m_forwardMapping = updatedForwardMapping;
    syncReverseMapping();
  }
    break;

  case Transformation::Kind::Collapse:
  {
    Identifier target = transformation.target();
    int stmtNb = maximumAt(target) + 1;
    CLINT_ASSERT((stmtNb % 2) == 0,
                 "A collapsable beta-prefix should have an even number of statements");

    IdentifierMultiMap updatedForwardMapping;
    for (auto m : m_forwardMapping) {
      Identifier identifier  = m.second;
      // Remove betas for second parts
      if (!(BetaUtility::isPrefix(target, identifier) && identifier.at(target.size()) >= stmtNb / 2)) {
        updatedForwardMapping.emplace(m.first, identifier);
      } else {
        m_createdMappings.erase(identifier);
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
      if (BetaUtility::isPrefixOrEqual(target, identifier)) {
        BetaUtility::createLoop(identifier, transformation.depth() + 1);
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

  case Transformation::Kind::Linearize:
  {
    Identifier target = transformation.target();
    IdentifierMultiMap updatedForwardMapping;
    std::set<Identifier> updatedCreatedMappings;

    for (auto m : m_forwardMapping) {
      Identifier identifier = m.second;
      if (BetaUtility::isPrefixOrEqual(target, identifier)) {
        BetaUtility::removeLoop(identifier, transformation.depth() + 1);
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

  case Transformation::Kind::Embed:
  {
    Identifier target = transformation.target();
    IdentifierMultiMap updatedForwardMapping;
    std::set<Identifier> updatedCreatedMappings;

    for (auto m : m_forwardMapping) {
      Identifier identifier = m.second;
      if (BetaUtility::isEqual(identifier, target)) {
        BetaUtility::createLoop(identifier, identifier.size());
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

  case Transformation::Kind::Unembed:
  {
    Identifier target = transformation.target();
    IdentifierMultiMap updatedForwardMapping;
    std::set<Identifier> updatedCreatedMappings;

    CLINT_ASSERT(maximumAt(BetaUtility::firstPrefix(target)) == 0,
                 "Cannot unembed statement which is not alone in the loop");

    for (auto m : m_forwardMapping) {
      Identifier identifier = m.second;
      if (BetaUtility::isEqual(identifier, target)) {
        BetaUtility::removeLoop(identifier, identifier.size() - 1);
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
  case Transformation::Kind::Reshape:
  case Transformation::Kind::Densify:
    // Do not affect beta
    break;

  default:
    CLINT_UNREACHABLE;
  }
}

void ClayBetaMapper::apply(osl_scop_p scop, const TransformationGroup &group) {
  iterativeApply(group.transformations);
}

void ClayBetaMapper::apply(osl_scop_p scop, const TransformationSequence &sequence) {
  iterativeApply(sequence.groups);
}

void ClayBetaMapper::dump(std::ostream &out) const {
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
