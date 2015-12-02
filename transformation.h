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
    Reorder,
    IndexSetSplitting,
    Grain,
    Reverse,
    Interchange,
    Tile,
    Linearize,
    Reshape,
    Densify,
    Collapse,
    Embed,
    Unembed
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

  const std::vector<int> &parameters() const {
    return m_parameters;
  }

  const std::vector<int> &iterators() const {
    return m_iterators;
  }

  static Transformation constantShift(const std::vector<int> &beta, int dimension, int amount) {
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
    t.m_targetBeta     = beta;
    t.m_depthInner     = sourceDimension;
    t.m_depthOuter     = targetDimension;
    t.m_constantAmount = factor;
    return t;
  }

  static Transformation reshape(const std::vector<int> &beta, int outputDimension, int inputDimension, int factor) {
    CLINT_ASSERT(outputDimension <= beta.size(), "Dimension overflow");
    CLINT_ASSERT(inputDimension <= beta.size(), "Dimension overflow");
    CLINT_ASSERT(outputDimension >= 0, "Dimension underflow");
    CLINT_ASSERT(inputDimension >= 0, "Dimension underflow");
    Transformation t;
    t.m_kind           = Kind::Reshape;
    t.m_targetBeta     = beta;
    t.m_depthInner     = inputDimension;
    t.m_depthOuter     = outputDimension;
    t.m_constantAmount = factor;
    return t;
  }

  static Transformation issFromConstraint(const std::vector<int> &beta,
                                          const std::vector<int> &issConstraint,
                                          size_t nbInputDims) {
    CLINT_ASSERT(beta.size() >= 1, "Beta too short");
    CLINT_ASSERT(issConstraint.size() - 1 - nbInputDims > 0,
                 "Input dimension number is wrong");
    Transformation t;
    t.m_kind           = Kind::IndexSetSplitting;
    t.m_targetBeta     = beta;
    t.m_constantAmount = issConstraint.back();
    t.m_iterators      = std::vector<int>(std::begin(issConstraint) + 1,
                                         std::begin(issConstraint) + 1 + nbInputDims);
    t.m_parameters     = std::vector<int>(std::begin(issConstraint) + 1 + nbInputDims,
                                          std::end(issConstraint) - 1);
    return t;

  }

  static Transformation grain(const std::vector<int> &beta, int depth, int value) {
    CLINT_ASSERT(beta.size() >= 1, "Beta too short");
    Transformation t;
    t.m_kind           = Kind::Grain;
    t.m_targetBeta     = beta;
    t.m_constantAmount = value;
    t.m_depthOuter     = depth;
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
    t.m_depthOuter = beta.size();
    return t;
  }

  static Transformation reverse(const std::vector<int> &beta, int dimension) {
    CLINT_ASSERT(beta.size() >= 1, "Cannot operate on the whole program");
    Transformation t;
    t.m_kind       = Kind::Reverse;
    t.m_targetBeta = beta;
    t.m_depthOuter = dimension;
    return t;
  }

  static Transformation interchange(const std::vector<int> &beta, int dimension1, int dimension2) {
    CLINT_ASSERT(beta.size() >= std::max(dimension1, dimension2),
                 "Beta too short");
    Transformation t;
    t.m_kind       = Kind::Interchange;
    t.m_targetBeta = beta;
    t.m_depthInner = dimension1;
    t.m_depthOuter = dimension2;
    return t;
  }

  static Transformation tile(const std::vector<int> &beta, int dimension, int size) {
    CLINT_ASSERT(beta.size() >= dimension,
                 "Beta too short");
    Transformation t;
    t.m_kind           = Kind::Tile;
    t.m_targetBeta     = beta;
    t.m_depthOuter     = dimension;
    t.m_constantAmount = size;
    return t;
  }

  static Transformation linearize(const std::vector<int> &beta, int dimension) {
    CLINT_ASSERT(beta.size() >= dimension,
                 "Beta too short");
    Transformation t;
    t.m_kind           = Kind::Linearize;
    t.m_targetBeta     = beta;
    t.m_depthOuter     = dimension;
    return t;
  }

  static Transformation densify(const std::vector<int> &beta, int dimension) {
    CLINT_ASSERT(beta.size() >= dimension, "Beta too short");
    Transformation t;
    t.m_kind           = Kind::Densify;
    t.m_targetBeta     = beta;
    t.m_depthOuter     = dimension;
    return t;
  }

  static Transformation embed(const std::vector<int> &beta) {
    Transformation t;
    t.m_kind           = Kind::Embed;
    t.m_targetBeta     = beta;
    return t;
  }

  static Transformation unembed(const std::vector<int> &beta) {
    Transformation t;
    t.m_kind           = Kind::Unembed;
    t.m_targetBeta     = beta;
    return t;
  }

  static Transformation collapse(const std::vector<int> &beta) {
    Transformation t;
    t.m_kind           = Kind::Collapse;
    t.m_targetBeta     = beta;
    return t;
  }

  /*+*********** raw transformations from parser  *************/
  static Transformation rawSplit(const std::vector<int> &beta, int dimension) {
    CLINT_ASSERT(beta.size() > 0, "Split transformation beta too short");
    CLINT_ASSERT(dimension <= beta.size(), "Split transformation dimension overflow");
    CLINT_ASSERT(dimension > 0, "Split transformation dimension underflow");
    Transformation t;
    t.m_kind       = Kind::Split;
    t.m_targetBeta = beta;
    t.m_depthOuter = dimension;
    return t;
  }

  static Transformation rawReorder(const std::vector<int> &beta, const std::vector<int> &order) {
    Transformation t;
    t.m_kind       = Kind::Reorder;
    t.m_targetBeta = beta;
    t.m_order      = order;
    return t;
  }

  static Transformation rawShift(const std::vector<int> &beta, int depth, const std::vector<int> &parameters, int constant) {
    CLINT_ASSERT(beta.size() > 0, "Shift transformation beta too short");
    CLINT_ASSERT(depth <= beta.size(), "Shift transformation depth overflow");
    CLINT_ASSERT(depth > 0, "Shift transformation depth underflow");
    Transformation t;
    t.m_kind           = Kind::Shift;
    t.m_targetBeta     = beta;
    t.m_depthOuter     = depth;
    t.m_parameters     = parameters;
    t.m_constantAmount = constant;
    return t;
  }

  static Transformation rawTile(const std::vector<int> &beta, int innerDepth, int outerDepth, int size, int pretty) {
    CLINT_ASSERT(beta.size() > 0, "Tile transformation beta too short");
    (void) pretty;
    (void) outerDepth;
    return Transformation::tile(beta, innerDepth, size);
  }

  static Transformation rawIss(const std::vector<int> &beta, const std::vector<std::vector<int>> &list) {
    CLINT_ASSERT(beta.size() > 0, "Iss transformation beta too short");
    Transformation t;
    t.m_kind       = Kind::IndexSetSplitting;
    t.m_targetBeta = beta;
    unwrapClayList(list, t);
    return t;
  }

private:
  std::vector<int> m_targetBeta;
  int m_depthInner, m_depthOuter;
  int m_constantAmount;
  std::vector<int> m_order;

  std::vector<int> m_parameters;
  std::vector<int> m_iterators;

  Kind m_kind;

  static void unwrapClayList(const std::vector<std::vector<int>> &list, Transformation &t) {
    CLINT_ASSERT(list.size() == 4, "List malformed");
    CLINT_ASSERT(list[3].size() <= 1, "More than one value for constant part of the list");
    CLINT_ASSERT(list[0].size() != 0 || list[1].size() != 0 || list[2].size() != 0,
                "Empty list in transformation");
    t.m_iterators      = list[0];
    t.m_parameters     = list[2];
    t.m_constantAmount = list[3].size() == 1 ? list[3][0] : 0;
  }
};

struct TransformationGroup {
  std::vector<Transformation> transformations;
};

struct TransformationSequence {
  std::vector<TransformationGroup> groups;
};

#endif // TRANSFORMATION_H
