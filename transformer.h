#ifndef TRANSFORMER_H
#define TRANSFORMER_H

#include "transformation.h"
#include "oslutils.h" // FIXME: move clay-beta-conversion to here and remove this include

class ClintScop;
class ClintStmtOccurrence;

#include <osl/osl.h>

#include <clay/clay.h>
#include <clay/options.h>
#include <clay/transformation.h>

#include <exception>
#include <map>
#include <set>
#include <unordered_map>

class Transformer {
public:
  virtual void apply(osl_scop_p scop, const TransformationSequence &sequence) {
    for (TransformationGroup group : sequence.groups) {
      apply(scop, group);
    }
  }

  virtual void apply(osl_scop_p scop, const TransformationGroup &group) {
    for (Transformation transformation : group.transformations) {
      apply(scop, transformation);
    }
  }

  virtual void apply(osl_scop_p scop, const Transformation &transformation) = 0;
  virtual std::vector<int> transformedBeta(const std::vector<int> &beta, const Transformation &transformation) = 0;
  virtual std::vector<int> originalBeta(const std::vector<int> &beta, const Transformation &transformation) = 0;
  virtual ~Transformer() {}
};

class ClayTransformer : public Transformer {
  class ClayBeta {
  public:
    ClayBeta(const std::vector<int> &vector) {
      beta = clayBetaFromVector(vector);
    }
    ~ClayBeta() {
      clay_array_free(beta);
    }

    operator clay_array_p() {
      return beta;
    }

  private:
    clay_array_p beta;
  };

public:
  ClayTransformer() {
    m_options = clay_options_malloc();
    m_options->keep_extbody = 1;
    m_options->normalize = 1;
    // TODO: check if --nocandl can be passed here, or is it necessary at all
  }

  void apply(osl_scop_p scop, const Transformation &transformation) override;
  std::vector<int> transformedBeta(const std::vector<int> &beta, const Transformation &transformation) override;
  std::vector<int> originalBeta(const std::vector<int> &beta, const Transformation &transformation) override;

  ~ClayTransformer() {
    clay_options_free(m_options);
  }

private:
  clay_options_p m_options;
};

class ClayBetaMapper : public Transformer {
public:
  enum MapError {
    SUCCESS = 0,
    NOT_ORIGINAL_BETA = 1,
    NO_MAPPING = 2,
    NOT_MAPPED_BETA = 3,
    NO_REVERSE = 4
  };

  ClayBetaMapper(ClintScop *scop);

  virtual ~ClayBetaMapper() {}

  void apply(osl_scop_p scop, const Transformation &transformation) override;
  void apply(osl_scop_p scop, const TransformationGroup &group) override;
  void apply(osl_scop_p scop, const TransformationSequence &sequence) override;
  std::vector<int> transformedBeta(const std::vector<int> &beta, const Transformation &transformation) override { return std::vector<int>(); }
  std::vector<int> originalBeta(const std::vector<int> &beta, const Transformation &transformation) override { return std::vector<int>(); }

  bool isOriginal(const std::vector<int> &beta) {
    return m_originalOccurrences.find(beta) != std::end(m_originalOccurrences);
  }

  bool isMapped(const std::vector<int> &beta) {
    auto occurrence = m_originalOccurrences.find(beta);
    if (occurrence == std::end(m_originalOccurrences))
      return false;
    return m_updatedBeta.find(occurrence->second) != std::end(m_updatedBeta);
  }

  std::pair<std::vector<int>, int> map(const std::vector<int> &beta) {
    auto occurrenceIter = m_originalOccurrences.find(beta);
    if (occurrenceIter == std::end(m_originalOccurrences))
      return std::make_pair(std::vector<int>(), NOT_ORIGINAL_BETA);
    auto betaIter = m_updatedBeta.find(occurrenceIter->second);
    if (betaIter == std::end(m_updatedBeta))
      return std::make_pair(std::vector<int>(), NO_MAPPING);
    return std::make_pair(betaIter->second, SUCCESS);
  }

  std::pair<std::vector<int>, int> reverseMap(const std::vector<int> &beta) {
    auto occurrenceIter = m_updatedOccurrences.find(beta);
    if (occurrenceIter == std::end(m_updatedOccurrences))
      return std::make_pair(std::vector<int>(), NOT_MAPPED_BETA);
    auto betaIter = m_originalBeta.find(occurrenceIter->second);
    if (betaIter == std::end(m_originalBeta))
      return std::make_pair(std::vector<int>(), NO_REVERSE);
    return std::make_pair(betaIter->second, SUCCESS);
  }

  template <typename T>
  void iterativeApply(osl_scop_p scop, const std::vector<T> &list) {
    bool first = true;
    decltype(m_originalBeta) originalBetaInit = m_originalBeta;
    decltype(m_originalOccurrences) originalOccurrencesInit = m_originalOccurrences;
    for (const T &t : list) {
      if (first) {
        first = false;
      } else {
        m_originalBeta = m_updatedBeta;
        m_originalOccurrences = m_updatedOccurrences;
        m_updatedBeta.clear();
        m_updatedOccurrences.clear();
      }
      apply(scop, t);
    }
    m_originalBeta = originalBetaInit;
    m_originalOccurrences = originalOccurrencesInit;
  }

  void resetOriginals(ClintScop *scop);
private:
  ClintScop *m_cscop;
  std::unordered_map<ClintStmtOccurrence *, std::vector<int>> m_originalBeta, m_updatedBeta;
  std::map<std::vector<int>, ClintStmtOccurrence *> m_originalOccurrences, m_updatedOccurrences;

  int m_lastOccurrenceFakePtr = 1;
};

class StmtLoopPosition {
public:
  virtual unsigned depth() = 0;
  virtual bool isLoop() = 0;
  virtual bool isStmt() = 0;
protected:
  ClintScop *m_cscop;
};

class BetaVector : public StmtLoopPosition {
  explicit BetaVector(const std::vector<int> &betaVector) :
    m_betaVector(betaVector) {
  }

  BetaVector(const BetaVector &other) {
    m_betaVector = other.m_betaVector;
  }

  virtual unsigned depth() override {
    return m_betaVector.size();
  }

  virtual bool isLoop() override {
    CLINT_UNREACHABLE;
  }

  virtual bool isStmt() override {
    return !isLoop();
  }

private:
  std::vector<int> m_betaVector;
};

class ClayBetaMapper2 : public Transformer {
private:
  typedef std::vector<int> Identifier;
  typedef std::multimap<Identifier, Identifier> IdentifierMultiMap;

  // TODO: extract these to beta-related classes afterwards
  int partialMatch(const Identifier &first, const Identifier &second) {
    int pos = 0;
    for (size_t i = 0, e = std::min(first.size(), second.size()); i < e; i++) {
      if (first[i] == second[i]) {
        ++pos;
      } else {
        break;
      }
    }
    return pos;
  }

  bool isPrefix(const Identifier &prefix, const Identifier &identifier) {
    if (prefix.size() >= identifier.size())
      return false;
    return partialMatch(prefix, identifier) == prefix.size();
  }

  bool isPrefixOrEqual(const Identifier &prefix, const Identifier &identifier) {
    if (prefix.size() > identifier.size())
      return false;
    return partialMatch(prefix, identifier) == prefix.size();
  }

  bool isEqual(const Identifier &first, const Identifier &second) {
    return first.size() == second.size() &&
           partialMatch(first, second) == first.size();
  }

  /// Second follows first
  bool follows(const Identifier &first, const Identifier &second) {
    int matchPosition = partialMatch(first, second);
    // If one is a prefix of another, then second is not following first.
    if (matchPosition == first.size() || matchPosition == second.size())
      return false;
    if (matchPosition < second.size() && matchPosition < first.size())
      return second.at(matchPosition) > first.at(matchPosition);
    return false;
  }

  /// Identical before depth; follows after
  bool followsAt(const Identifier &first, const Identifier &second, size_t depth) {
    if (depth >= first.size())
      throw std::overflow_error("Depth overflow for first identifier");
    if (depth >= second.size())
      throw std::overflow_error("Depth overflow for second identifier");
    int matchingLength = partialMatch(first, second);
    if (matchingLength < depth) {
      return false;
    }
    for (size_t i = depth, e = std::min(first.size(), second.size()); i < e; ++i) {
      if (first.at(i) < second.at(i))
        return true;
      else if (first.at(i) > second.at(i))
        return false;
      // else continue;
    }
    return false;
  }

  enum class IdentifierRelation {
    Equals,
    FirstPrefix,
    SecondPrefix,
    FirstFollows,
    SecondFollows
  };

  IdentifierRelation relation(const Identifier &first, const Identifier &second) {
    int matchPosition = partialMatch(first, second);
    if (matchPosition == first.size() && matchPosition == second.size())
      return IdentifierRelation::Equals;
    if (first.size() > second.size()) {
      if (matchPosition == second.size()) {
        return IdentifierRelation::SecondPrefix;
      } else if (matchPosition < second.size()) {
        return second.at(matchPosition) > first.at(matchPosition) ?
              IdentifierRelation::SecondFollows :
              IdentifierRelation::FirstFollows;
      }
    } else {
      if (matchPosition == first.size()) {
        return IdentifierRelation::FirstPrefix;
      } else if (matchPosition < first.size()) {
        return second.at(matchPosition) > first.at(matchPosition) ?
              IdentifierRelation::SecondFollows :
              IdentifierRelation::FirstFollows;
      }
    }
    CLINT_UNREACHABLE;
  }

  void createLoop(Identifier &identifier, size_t depth) {
    if (depth <= identifier.size())
      identifier.insert(std::begin(identifier) + depth, 0);
  }

  void appendStmt(Identifier &identifier, size_t value) {
    identifier.back() = value;
  }

  void changeOrderAt(Identifier &identifier, int order, size_t depth = static_cast<size_t>(-1)) {
    if (depth == static_cast<size_t>(-1)) {
      identifier.back() = order;
      return;
    }
    if (depth >= identifier.size()) {
      throw std::overflow_error("Depth overflow");
    }
    *(std::begin(identifier) + depth) = order;
  }

  int orderAt(Identifier &identifier, size_t depth = static_cast<size_t>(-1)) {
    if (depth == static_cast<size_t>(-1))
      return identifier.back();
    if (depth >= identifier.size()) {
      throw std::overflow_error("Depth overflow");
    }
    return identifier.at(depth);
  }

  void nextInLoop(Identifier &identifier, size_t depth = static_cast<size_t>(-1)) {
    changeOrderAt(identifier, orderAt(identifier, depth) + 1, depth);
  }

  void prevInLoop(Identifier &identifier, size_t depth = static_cast<size_t>(-1)) {
    changeOrderAt(identifier, orderAt(identifier, depth) - 1, depth);
  }

  // Look in mappend betas
  int maximumAt(const Identifier &prefix) {
    int maximum = INT_MIN;
    for (auto p : m_forwardMapping) {
      int matchingLength = partialMatch(prefix, p.second);
      if (matchingLength == prefix.size() && p.second.size() >= prefix.size()) {
        maximum = std::max(maximum, p.second.at(prefix.size()));
      }
    }
    return maximum;
  }

  // Move to ClintScop
  int countMatches(Identifier target);

public:
  explicit ClayBetaMapper2(ClintScop *scop);
  virtual ~ClayBetaMapper2();

  void apply(osl_scop_p scop, const Transformation &transformation) override;
  void apply(osl_scop_p scop, const TransformationGroup &group) override;
  void apply(osl_scop_p scop, const TransformationSequence &sequence) override;

  // TODO: Bullshit functions, to be removed.
  std::vector<int> transformedBeta(const std::vector<int> &beta, const Transformation &transformation) override { return std::vector<int>(); }
  std::vector<int> originalBeta(const std::vector<int> &beta, const Transformation &transformation) override { return std::vector<int>(); }

  std::set<Identifier> forwardMap(const Identifier &identifier) const {
    return std::move(map(identifier, m_forwardMapping));
  }

  std::set<Identifier> reverseMap(const Identifier &identifier) const {
    return std::move(map(identifier, m_reverseMapping));
  }
private:
  std::set<Identifier> map(const Identifier &identifier, const IdentifierMultiMap &mapping) const {
    typename IdentifierMultiMap::const_iterator beginIt, endIt;
    std::tie(beginIt, endIt) = mapping.equal_range(identifier);
    std::set<Identifier> result;
    for (auto it = beginIt; it != endIt; it++) {
      result.insert(it->second);
    }
    return result;
  }

  template <typename T>
  void iterativeApply(const std::vector<T> &list) {
    for (const T &t : list) {
      apply(nullptr, t);
    }
  }

  void replaceMappings(Identifier original, Identifier oldModified, Identifier newModified);
  void addMappings(Identifier original, Identifier modified);
  void removeMappings(Identifier original, Identifier modified);

  void syncReverseMapping() {
    m_reverseMapping.clear();
    for (auto v : m_forwardMapping) {
      m_reverseMapping.emplace(v.second, v.first);
    }
  }

  IdentifierMultiMap m_forwardMapping;
  IdentifierMultiMap m_reverseMapping;

  // A set of all identifiers that were created during the mapping process
  // and did not originally exist in the scop.
  std::set<Identifier> m_createdMappings;
};

class ClayScriptGenerator : public Transformer {
public:
  ClayScriptGenerator(std::ostream &stream) : m_stream(stream){}
  ~ClayScriptGenerator() {}

  void apply(osl_scop_p scop, const Transformation &transformation) override {
    switch(transformation.kind()) {
    case Transformation::Kind::Fuse:
      m_stream << "fuse([";
      outputVector(m_stream, transformation.target()) << "]);\n";
      break;
    case Transformation::Kind::Split:
      m_stream << "split([";
      outputVector(m_stream, transformation.target()) << "], " << transformation.depth() << ");\n";
      break;
    case Transformation::Kind::Reorder:
      m_stream << "reorder([";
      outputVector(m_stream, transformation.target()) << "], [";
      outputVector(m_stream, transformation.order()) << "]);\n";
      break;
    case Transformation::Kind::Shift:
      m_stream << "shift([";
      outputVector(m_stream, transformation.target()) << "], "
             << transformation.depth() << ", "
             << "{" << transformation.constantAmount() << "});\n";
      break;
    case Transformation::Kind::IndexSetSplitting:
      m_stream << "iss([";
      outputVector(m_stream, transformation.target()) << "], {";
      outputVector(m_stream, transformation.iterators()) << " | ";
      outputNonZeroVector(m_stream, transformation.parameters()) << " | ";
      m_stream << transformation.constantAmount() << "});\n";
      break;
    case Transformation::Kind::Grain:
      m_stream << "grain([";
      outputVector(m_stream, transformation.target()) << "], "
          << transformation.depth() << ", "
          << transformation.constantAmount() << ");\n";
      break;
    case Transformation::Kind::Reverse:
      m_stream << "reverse([";
      outputVector(m_stream, transformation.target()) << "], "
          << transformation.depth() << ");\n";
      break;
    case Transformation::Kind::Skew:
//      m_stream << "skew([";
//      outputVector(m_stream, transformation.target()) << "], "
//          << transformation.depth() << ", "
//          << transformation.constantAmount() << ");\n";
      m_stream << "shift([";
      outputVector(m_stream, transformation.target()) << "], "
          << transformation.depth() << ", {";
      for (int i = 0, e = std::max(transformation.depth(), transformation.secondDepth()); i < e; ++i) {
        if (i != 0) m_stream << ",";

        if (i + 1 == transformation.depth())            m_stream << "1";
        else if (i + 1 == transformation.secondDepth()) m_stream << -transformation.constantAmount();
        else                                            m_stream << "0";
      }
      m_stream << "||});\n";
      break;

    case Transformation::Kind::Interchange:
      m_stream << "interchange([";
      outputVector(m_stream, transformation.target()) << "], "
         << transformation.depth() << ", "
         << transformation.secondDepth() << ", 1);\n";
      break;

    case Transformation::Kind::Tile:
      m_stream << "tile([";
      outputVector(m_stream, transformation.target()) << "], "
         << transformation.depth() << ", "
         << transformation.depth() << ", "
         << transformation.constantAmount() << ", "
         << "0);\n";
      break;

    default:
      m_stream << "###unkonwn transformation###\n";
    }
  }

  std::vector<int> transformedBeta(const std::vector<int> &beta, const Transformation &transformation) override { return std::vector<int>(); }
  std::vector<int> originalBeta(const std::vector<int> &beta, const Transformation &transformation) override { return std::vector<int>(); }

private:
  std::ostream &m_stream;

  template <typename T>
  std::ostream &outputVector(std::ostream &stream, const std::vector<T> &vector) {
    if (vector.size() == 0)
      return stream;

    std::copy(std::begin(vector), std::end(vector) - 1, std::ostream_iterator<T>(stream, ", "));
    stream << vector.back();
    return stream;
  }

  template <typename T>
  std::ostream &outputNonZeroVector(std::ostream &stream, const std::vector<T> &vector) {
    if (std::all_of(std::begin(vector), std::end(vector),
                    std::bind(std::equal_to<int>(), std::placeholders::_1, 0)))
      return stream;
    return outputVector(stream, vector);
  }
};

#endif // TRANSFORMER_H
