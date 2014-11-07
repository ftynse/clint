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


#endif // TRANSFORMER_H
