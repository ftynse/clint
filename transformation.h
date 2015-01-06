#ifndef TRANSFORMATION_H
#define TRANSFORMATION_H

#include <vector>
#include <algorithm>

#include "macros.h"

class Transformation {
public:
  enum class Kind {
    Shift,
    Skew,
    Fuse,
    Split,
    Reorder
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

  const std::vector<int> &order() const {
    return m_order;
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

  static Transformation putAfterLast(const std::vector<int> &beta, int size) {
    CLINT_ASSERT(beta.size() >= 1, "Cannot operate on the whole program");
    return putAfter(beta, size - 1, size);
//    Transformation t;
//    t.m_kind = Kind::Reorder;
//    t.m_targetBeta = std::vector<int>(std::begin(beta), std::end(beta) - 1);
//    t.m_order.resize(size);
//    for (int i = 0; i < size; i++) {
//      if (i < beta.back())
//        t.m_order[i] = i;
//      else if (i == beta.back())
//        t.m_order[i] = size - 1;
//      else
//        t.m_order[i] = i - 1;
//    }
//    return t;
  }

  static Transformation putAfter(const std::vector<int> &beta, int position, int size) {
    CLINT_ASSERT(beta.size() >= 1, "Cannot operate on the whole program");
    CLINT_ASSERT(size > position, "Position of the element to put after overflow");
    if (beta.back() > position) {
      std::vector<int> b(beta);
      int tmp = position;
      position = b.back();
      b.back() = tmp;
      return putAfter(b, position, size);
    }
//    if (beta.back() > position) {
//      return putBefore(beta, position + 1, size);
//    }

    Transformation t;
    t.m_kind = Kind::Reorder;
    t.m_targetBeta = std::vector<int>(std::begin(beta), std::end(beta) - 1);
    t.m_order.resize(size);
    for (int i = 0; i < size; i++) {
      if (i > beta.back() && i <= position)
        t.m_order[i] = i - 1;
      else if (i == beta.back())
        t.m_order[i] = position;
      else
        t.m_order[i] = i;
    }
    return t;
  }

  static Transformation putBefore(const std::vector<int> &beta, int position, int size) {
    CLINT_ASSERT(beta.size() >= 1, "Cannot operate on the whole program");
    CLINT_ASSERT(size >= position, "Position of the element to put after overflow");
    if (position > beta.back()) {
      return putAfter(beta, position - 1, size);
    }

    Transformation t;
    t.m_kind = Kind::Reorder;
    t.m_targetBeta = std::vector<int>(std::begin(beta), std::end(beta) - 1);
    t.m_order.resize(size);
    for (int i = 0; i < size; i++) {
      if (i >= position && i < beta.back()) {
        t.m_order[i] = i + 1;
      } else if (i == beta.back()) {
        t.m_order[i] = position;
      } else {
        t.m_order[i] = i;
      }
    }
    return t;
  }

  static Transformation splitAfter(const std::vector<int> &beta) {
    CLINT_ASSERT(beta.size() >= 1, "Cannot operate on the whole program");
    Transformation t;
    t.m_kind       = Kind::Split;
    t.m_targetBeta = beta;
    t.m_depthOuter = beta.size() - 1;
    return t;
  }

  static Transformation fuseNext(const std::vector<int> &beta) {
    CLINT_ASSERT(beta.size() >= 1, "Cannot operate on the whole program");
    Transformation t;
    t.m_kind       = Kind::Fuse;
    t.m_targetBeta = beta;
    return t;
  }

private:
  std::vector<int> m_targetBeta;
  int m_depthInner, m_depthOuter;
  int m_constantAmount;
  std::vector<int> m_order;

  Kind m_kind;
};

struct TransformationGroup {
  std::vector<Transformation> transformations;
};

struct TransformationSequence {
  std::vector<TransformationGroup> groups;
};

#endif // TRANSFORMATION_H
