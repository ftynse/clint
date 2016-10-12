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

#include <climits>
#include <exception>
#include <iterator>
#include <map>
#include <set>
#include <unordered_map>

#include <boost/optional.hpp>

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

  virtual boost::optional<Transformation> guessInverseTransformation(osl_scop_p, const Transformation &transformation) {
    throw std::domain_error("This class does not provide implementarion of guessInverseTransformation");
    CLINT_UNREACHABLE;
  }

  virtual void apply(osl_scop_p scop, const Transformation &transformation) = 0;
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

  boost::optional<Transformation> guessInverseTransformation(osl_scop_p scop, const Transformation &transformation) override;
  void apply(osl_scop_p scop, const Transformation &transformation) override;

  ~ClayTransformer() {
    clay_options_free(m_options);
  }

private:
  clay_options_p m_options;
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

class BetaUtility {
public:
  typedef std::vector<int> Identifier;
  typedef std::multimap<Identifier, Identifier> IdentifierMultiMap;

  static size_t depth(const Identifier &identifier) {
    return identifier.size();
  }

  static bool isPrefix(const Identifier &prefix, const Identifier &identifier) {
    if (prefix.size() >= identifier.size())
      return false;
    return partialMatch(prefix, identifier) == prefix.size();
  }

  static bool isPrefixOrEqual(const Identifier &prefix, const Identifier &identifier) {
    if (prefix.size() > identifier.size())
      return false;
    return partialMatch(prefix, identifier) == prefix.size();
  }

  static bool isEqual(const Identifier &first, const Identifier &second) {
    return first.size() == second.size() &&
           partialMatch(first, second) == first.size();
  }

  static Identifier firstPrefix(const Identifier &identifier) {
    if (identifier.size() == 0)
      return identifier;
    return Identifier(std::begin(identifier), std::end(identifier) - 1);
  }

  static int partialMatch(const Identifier &first, const Identifier &second) {
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

  /// Second follows first
  static bool follows(const Identifier &first, const Identifier &second) {
    int matchPosition = partialMatch(first, second);
    // If one is a prefix of another, then second is not following first.
    if (matchPosition == first.size() || matchPosition == second.size())
      return false;
    if (matchPosition < second.size() && matchPosition < first.size())
      return second.at(matchPosition) > first.at(matchPosition);
    return false;
  }

  /// Identical before depth; follows after
  static bool followsAt(const Identifier &first, const Identifier &second, size_t depth) {
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

  static IdentifierRelation relation(const Identifier &first, const Identifier &second) {
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

  static void createLoop(Identifier &identifier, size_t depth) {
    if (depth <= identifier.size())
      identifier.insert(std::begin(identifier) + depth, 0);
  }

  static void removeLoop(Identifier &identifier, size_t depth) {
    if (depth < identifier.size())
      identifier.erase(std::begin(identifier) + depth);
  }

  static void appendStmt(Identifier &identifier, size_t value) {
    identifier.back() = value;
  }

  static void changeOrderAt(Identifier &identifier, int order, size_t depth = static_cast<size_t>(-1)) {
    if (depth == static_cast<size_t>(-1)) {
      identifier.back() = order;
      return;
    }
    if (depth >= identifier.size()) {
      throw std::overflow_error("Depth overflow");
    }
    *(std::begin(identifier) + depth) = order;
  }

  static int orderAt(Identifier &identifier, size_t depth = static_cast<size_t>(-1)) {
    if (depth == static_cast<size_t>(-1))
      return identifier.back();
    if (depth >= identifier.size()) {
      throw std::overflow_error("Depth overflow");
    }
    return identifier.at(depth);
  }

  static void nextInLoop(Identifier &identifier, size_t depth = static_cast<size_t>(-1)) {
    changeOrderAt(identifier, orderAt(identifier, depth) + 1, depth);
  }

  static void prevInLoop(Identifier &identifier, size_t depth = static_cast<size_t>(-1)) {
    changeOrderAt(identifier, orderAt(identifier, depth) - 1, depth);
  }

};

class ClayBetaMapper : public Transformer {
private:
  typedef std::vector<int> Identifier;
  typedef std::multimap<Identifier, Identifier> IdentifierMultiMap;

  // Look in mappend betas
  int maximumAt(const Identifier &prefix) {
    int maximum = INT_MIN;
    for (auto p : m_forwardMapping) {
      int matchingLength = BetaUtility::partialMatch(prefix, p.second);
      if (matchingLength == prefix.size() && p.second.size() >= prefix.size()) {
        maximum = std::max(maximum, p.second.at(prefix.size()));
      }
    }
    return maximum;
  }

public:
  explicit ClayBetaMapper(ClintScop *scop);
  virtual ~ClayBetaMapper();

  void apply(osl_scop_p scop, const Transformation &transformation) override;
  void apply(osl_scop_p scop, const TransformationGroup &group) override;
  void apply(osl_scop_p scop, const TransformationSequence &sequence) override;

  void dump(std::ostream &out) const;

  void reset();

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
  // TODO: is it useful somehow? does not seem to be referenced outside the class that keeps it up-to-date
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
      // FIXME: workaround for old variable shift definition
      if (transformation.iterators().size() != 0) {
        const std::vector<int> &iterators = transformation.iterators();
        for (size_t i = 0; i < iterators.size(); i++) {
          if (iterators[i] != 0) {
            m_stream << "skew([";
            outputVector(m_stream, transformation.target()) << "], "
                  << transformation.depth() << ", "
                  << transformation.secondDepth() << ", "
                  << transformation.constantAmount() << ");\n";
          }
        }
      }

      m_stream << "shift([";
      outputVector(m_stream, transformation.target()) << "], "
             << transformation.depth() << ", [";
      outputVector(m_stream, transformation.parameters()) << "], "
             << transformation.constantAmount() << ");\n";
      break;
    case Transformation::Kind::IndexSetSplitting:
      m_stream << "iss([";
      outputVector(m_stream, transformation.target()) << "], {";
      outputVector(m_stream, transformation.iterators()) << " || ";
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
      m_stream << "skew([";
      outputVector(m_stream, transformation.target()) << "], "
          << transformation.depth() << ", " << transformation.secondDepth()
          << ", " << -transformation.constantAmount() << ");\n";
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

    case Transformation::Kind::Linearize:
      m_stream << "linearize([";
      outputVector(m_stream, transformation.target()) << "], "
         << transformation.depth() << ");\n";
      break;

    case Transformation::Kind::Reshape:
      m_stream << "reshape([";
      outputVector(m_stream, transformation.target()) << "], "
         << transformation.depth() << ", "
         << transformation.secondDepth() << ", "
         << transformation.constantAmount() << ");\n";
      break;

    case Transformation::Kind::Densify:
      m_stream << "densify([";
      outputVector(m_stream, transformation.target()) << "], "
         << transformation.depth() << ");\n";
      break;

    case Transformation::Kind::Collapse:
      m_stream << "collapse([";
      outputVector(m_stream, transformation.target()) << "]);\n";
      break;

    case Transformation::Kind::Embed:
      m_stream << "embed([";
      outputVector(m_stream, transformation.target()) << "]);\n";
      break;

    case Transformation::Kind::Unembed:
      m_stream << "unembed([";
      outputVector(m_stream, transformation.target()) << "]);\n";
      break;

    default:
      m_stream << "###unkonwn transformation###\n";
    }
  }

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
