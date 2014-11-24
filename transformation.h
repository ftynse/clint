#ifndef TRANSFORMATION_H
#define TRANSFORMATION_H

#include <vector>

#include "macros.h"

class Transformation {
public:
  enum class Kind {
    Shift,
    Skew,
    Fuse,
    Split
  };

  Kind kind() const {
    return m_kind;
  }

  const std::vector<int> &target() const {
    return m_targetBeta;
  }

  int depth() const {
    return m_depthOuter;
  }

  int secondDepth() const {
    return m_depthInner;
  }

  int constantAmount() const {
    return m_constantAmount;
  }

  static Transformation consantShift(const std::vector<int> &beta, int dimension, int amount) {
    CLINT_ASSERT(dimension <= beta.size(), "Dimension overflow");
    Transformation t;
    t.m_kind           = Kind::Shift;
    t.m_targetBeta     = beta;
    t.m_depthOuter     = dimension;
    t.m_constantAmount = amount;
    return t;
  }

  static Transformation skew(const std::vector<int> &beta, int sourceDimension, int targetDimension, int factor) {
    CLINT_ASSERT(sourceDimension <= beta.size(), "Dimension overflow");
    CLINT_ASSERT(targetDimension <= beta.size(), "Dimension overflow");
    Transformation t;
    t.m_kind           = Kind::Skew;
    t.m_depthInner     = sourceDimension;
    t.m_depthOuter     = targetDimension;
    t.m_constantAmount = factor;
    return t;
  }

private:
  std::vector<int> m_targetBeta;
  int m_depthInner, m_depthOuter;
  int m_constantAmount;

  Kind m_kind;
};

struct TransformationGroup {
  std::vector<Transformation> transformations;
};

struct TransformationSequence {
  std::vector<TransformationGroup> groups;
};

#endif // TRANSFORMATION_H
