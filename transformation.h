#ifndef TRANSFORMATION_H
#define TRANSFORMATION_H

#include <vector>

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
