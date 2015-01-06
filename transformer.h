#ifndef TRANSFORMER_H
#define TRANSFORMER_H

#include "transformation.h"
#include "oslutils.h" // FIXME: move clay-beta-conversion to here and remove this include

#include <osl/osl.h>

#include <clay/clay.h>
#include <clay/options.h>
#include <clay/transformation.h>

class Transformer {
public:
  void apply(osl_scop_p scop, const TransformationSequence &sequence) {
    for (TransformationGroup group : sequence.groups) {
      apply(scop, group);
    }
  }

  void apply(osl_scop_p scop, const TransformationGroup &group) {
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
