#include "transformation.h"

bool Transformation::operator ==(const Transformation &other) const {
  if (other.m_kind != m_kind)
    return false;

  switch (m_kind) {
  case Kind::Fuse:
  case Kind::Embed:
  case Kind::Unembed:
  case Kind::Collapse:
    return other.m_targetBeta == m_targetBeta;
    break;

  case Kind::Shift:
    return other.m_targetBeta == m_targetBeta &&
           other.m_depthOuter == m_depthOuter &&
           other.m_parameters == m_parameters &&
           other.m_constantAmount == m_constantAmount;
    break;

  case Kind::Skew:
  case Kind::Reshape:
    return other.m_targetBeta == m_targetBeta &&
           other.m_depthOuter == m_depthOuter &&
           other.m_depthInner == m_depthInner &&
           other.m_constantAmount == m_constantAmount;
    break;

  case Kind::Split:
  case Kind::Reverse:
  case Kind::Linearize:
  case Kind::Densify:
    return other.m_targetBeta == m_targetBeta &&
           other.m_depthOuter == m_depthInner;
    break;

  case Kind::Reorder:
    return other.m_targetBeta == m_targetBeta &&
           other.m_order == m_order;
    break;

  case Kind::IndexSetSplitting:
    return other.m_targetBeta == m_targetBeta &&
           other.m_iterators  == m_iterators &&
           other.m_parameters == m_parameters &&
           other.m_constantAmount == m_constantAmount;
    break;

  case Kind::Grain:
  case Kind::Tile:
    return other.m_targetBeta == m_targetBeta &&
           other.m_depthOuter == m_depthOuter &&
           other.m_constantAmount == m_constantAmount;
    break;

  case Kind::Interchange:
    return other.m_targetBeta == m_targetBeta &&
           other.m_depthOuter == m_depthOuter &&
           other.m_depthInner == m_depthInner;
    break;
  }

  return false;
}

bool TransformationGroup::operator == (const TransformationGroup &other) const {
  return transformations == other.transformations;
}

bool TransformationGroup::operator != (const TransformationGroup &other) const {
  return !(this->operator ==(other));
}
