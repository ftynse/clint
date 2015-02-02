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

#include <map>
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
    {
      m_stream << "iss([";
      outputVector(m_stream, transformation.target()) << "], {";
      std::vector<int> iters;
      for (int i = 0, e = transformation.target().size(); i < e; i++)
        iters.push_back(transformation.depth() == i ?
                          (transformation.m_useFirstParameter ? 1 : -1) :
                          0);
      outputVector(m_stream, iters) << "|" << (transformation.m_useFirstParameter ? "-1" : "") << "|" <<
                                       (transformation.m_useFirstParameter ? transformation.constantAmount() : transformation.constantAmount() - 1) << "});\n";
    }
      break;
    case Transformation::Kind::Grain:
      m_stream << "grain([";
      outputVector(m_stream, transformation.target()) << "], "
          << transformation.depth() << ", "
          << transformation.constantAmount() << ");\n";
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
};

#endif // TRANSFORMER_H
