#include "vizmanipulationalphatransformation.h"

#include "vizpolyhedron.h"
#include "vizprojection.h"
#include "vizproperties.h"
#include "vizhandle.h"

static int absCeilDiv(double numerator, int denominator) {
  double ratio = denominator == 0 ? 0 : numerator / denominator;
  int sign = ratio < 0 ? -1 : 1;
  return sign * ceil(fabs(ratio));
}

VizManipulationAlphaTransformation::~VizManipulationAlphaTransformation() {
}

void VizManipulationAlphaTransformation::setDisplacement(QPointF displacement) {
  m_displacement = displacement;
}

QPointF VizManipulationAlphaTransformation::displacement() const {
  return m_displacement;
}

VizManipulationSkewing::VizManipulationSkewing(VizPolyhedron *polyhedron, int corner) :
  m_polyhedron(polyhedron), m_corner(corner) {

  m_startPosition = m_polyhedron->handle(m_polyhedron->maxDisplacementHandleKind())->center();
  m_targetPosition = m_polyhedron->animationTarget()->handle(m_polyhedron->maxDisplacementHandleKind())->center();
  m_displacementAtStart = QPointF(0, 0);
  m_previousDisplacement = QPointF(0, 0);
}

void VizManipulationSkewing::postAnimationCreate() {
  VizPolyhedron *animationTarget = m_polyhedron->animationTarget();
  m_startPosition = m_polyhedron->handle(m_polyhedron->maxDisplacementHandleKind())->center();
  m_displacementAtStart = m_displacement;
  m_targetPosition = animationTarget->handle(m_polyhedron->maxDisplacementHandleKind())->center();
}

void VizManipulationSkewing::postShadowCreate() {}

void VizManipulationSkewing::correctUntilValid(
    TransformationGroup &group,
    boost::optional<std::pair<TransformationGroup, TransformationGroup> > &replacedGroup) {
  replacedGroup = boost::none;
  for (int i = 0; i < 10; i++) {
    try {
      m_polyhedron->skipNextUpdate();
      m_polyhedron->occurrence()->scop()->transform(group);
      m_polyhedron->occurrence()->scop()->executeTransformationSequence();
      return;
    } catch (std::logic_error) {
      TransformationGroup original = i == 0 ? group : replacedGroup.value().first;
      m_polyhedron->occurrence()->scop()->discardLastTransformationGroup();
      for (size_t j = 0; j < group.transformations.size(); ++j) {
        const Transformation &transformation = group.transformations[j];
        if (transformation.kind() != Transformation::Kind::Reshape)
          continue;

        double pointDistance = m_polyhedron->coordinateSystem()->projection()->vizProperties()->pointDistance();
        int horzOffset = round(displacement().x() / pointDistance);
        int vertOffset = round(-displacement().y() / pointDistance);

        int sign = 0;
        if (transformation.secondDepth() - 1 == m_polyhedron->coordinateSystem()->verticalAxisLength()) {
          sign = horzOffset < 0 ? -1 : 1;
        } else {
          sign = vertOffset < 0 ? -1 : 1;
        }
        group.transformations[j] = Transformation::reshape(transformation.target(),
                  transformation.depth(), transformation.secondDepth(),
                  transformation.constantAmount() + sign);
      }
      replacedGroup = boost::make_optional(std::make_pair(original, group));
    }
  }
  CLINT_UNREACHABLE;
}

std::pair<TransformationGroup, TransformationGroup> VizManipulationSkewing::computeShadowAndAnimation() {
  TransformationGroup shadowGroup = computeShadowAnimationInternal(false);
  TransformationGroup animationGroup = computeShadowAnimationInternal(true);
  m_previousDisplacement = m_displacement;
  return std::make_pair(shadowGroup, animationGroup);
}

TransformationGroup VizManipulationSkewing::computeShadowAnimationInternal(bool isAnimation) {
  VizProperties *properties = m_polyhedron->coordinateSystem()->projection()->vizProperties();
  const double pointDistance = properties->pointDistance();
  double horzOffset = m_displacement.x() / pointDistance;
  double vertOffset = m_displacement.y() / pointDistance;

  int horizontalDimIdx = m_polyhedron->coordinateSystem()->horizontalDimensionIdx();
  int verticalDimIdx   = m_polyhedron->coordinateSystem()->verticalDimensionIdx();

  // Max is inclusive, so we would need +1 here, but we would also need -1 before division to compute the skew factor
  int horizontalRange = (m_polyhedron->occurrence()->maximumOrigValue(horizontalDimIdx) -
                         m_polyhedron->occurrence()->minimumOrigValue(horizontalDimIdx));
  int verticalRange =   (m_polyhedron->occurrence()->maximumOrigValue(verticalDimIdx) -
                         m_polyhedron->occurrence()->minimumOrigValue(verticalDimIdx));

  int verticalSkewFactor;
  int horizontalSkewFactor;

  if (isAnimation) {
    // Round to a larger absolute value (start transformation as soon as possible).
    verticalSkewFactor = absCeilDiv(-vertOffset, horizontalRange);
    horizontalSkewFactor = absCeilDiv(horzOffset, verticalRange);

    QPointF displacementSpeed = m_displacement - m_previousDisplacement;
    if (m_displacement.x() > 0 && displacementSpeed.x() < 0)
      horizontalSkewFactor -= 1;
    else if (m_displacement.x() < 0 && displacementSpeed.x() > 0)
      horizontalSkewFactor += 1;
    else if (m_displacement.y() < 0 && displacementSpeed.y() > 0)
      verticalSkewFactor -= 1;
    else if (m_displacement.y() > 0 && displacementSpeed.y() < 0)
      verticalSkewFactor += 1;
  } else {
    // Double rounding is deliberate.
    verticalSkewFactor = horizontalRange == 0 ? 0 : round(round(-vertOffset) / horizontalRange);
    horizontalSkewFactor = verticalRange == 0 ? 0 : round(round(horzOffset) / verticalRange);
  }

  if (!(m_corner & C_RIGHT)) verticalSkewFactor = -verticalSkewFactor;
  if (m_corner & C_BOTTOM) horizontalSkewFactor = -horizontalSkewFactor;

  size_t horizontalDepth = horizontalDimIdx != VizProperties::NO_DIMENSION ?
        m_polyhedron->occurrence()->depth(horizontalDimIdx) :
        VizProperties::NO_DIMENSION;
  size_t verticalDepth   = verticalDimIdx != VizProperties::NO_DIMENSION ?
        m_polyhedron->occurrence()->depth(verticalDimIdx) :
        VizProperties::NO_DIMENSION;

  TransformationGroup group;
  if (verticalSkewFactor != 0 || horizontalSkewFactor != 0) {
    // vertical
    if (verticalSkewFactor != 0) {
      group.transformations.push_back(Transformation::reshape(
                                        m_polyhedron->occurrence()->betaVector(),
                                        verticalDepth,
                                        m_polyhedron->coordinateSystem()->horizontalDimensionIdx() + 1,
                                        -verticalSkewFactor));
      // post-shift
      if (m_corner & C_BOTTOM) {
        group.transformations.push_back(Transformation::constantShift(
                                          m_polyhedron->occurrence()->betaVector(),
                                          verticalDepth,
                                          verticalSkewFactor * horizontalRange));
      }
    }
    // horizontal
    if (horizontalSkewFactor != 0) {
      group.transformations.push_back(Transformation::reshape(
                                        m_polyhedron->occurrence()->betaVector(),
                                        horizontalDepth,
                                        m_polyhedron->coordinateSystem()->verticalDimensionIdx() + 1, // input dim is not subject to tiling
                                        -horizontalSkewFactor));

      // post-shift
      if ((!m_corner & C_RIGHT)) {
        group.transformations.push_back(Transformation::constantShift(
                                          m_polyhedron->occurrence()->betaVector(),
                                          horizontalDepth,
                                          horizontalSkewFactor * verticalRange));
      }
    }

  }
  return group;
}

double VizManipulationSkewing::animationRatio() {
  QPointF offset = m_displacement - m_displacementAtStart;
  QLineF totalLine(m_startPosition, m_targetPosition);
  QLineF currentLine(m_startPosition, m_startPosition + offset);

  return totalLine.length() == 0 ? 0 : currentLine.length() / totalLine.length();
}

/*==========================================================================*/

VizManipulationShifting::VizManipulationShifting(VizPolyhedron *polyhedron) :
  m_polyhedron(polyhedron) {
}

void VizManipulationShifting::postAnimationCreate() {
}

void VizManipulationShifting::postShadowCreate() {
}

void VizManipulationShifting::correctUntilValid(TransformationGroup &group,
                                                boost::optional<std::pair<TransformationGroup, TransformationGroup>> &replacedGroup) {
  replacedGroup = boost::none;
  try {
    m_polyhedron->skipNextUpdate();
    m_polyhedron->occurrence()->scop()->transform(group);
    m_polyhedron->occurrence()->scop()->executeTransformationSequence();
    return;
  } catch (std::logic_error) {
    CLINT_UNREACHABLE;  // If happens, make this catch remove all shift transformations from the group.
  }
}

TransformationGroup VizManipulationShifting::computeShadowAnimationInternal(bool isAnimation) {
  int horzOffset, vertOffset;
  double pointDistance = m_polyhedron->coordinateSystem()->projection()->vizProperties()->pointDistance();
  if (isAnimation) {
    horzOffset = absCeilDiv(m_displacement.x(), pointDistance);
    vertOffset = absCeilDiv(-m_displacement.y(), pointDistance);
  } else {
    horzOffset = round(m_displacement.x() / pointDistance);
    vertOffset = round(-m_displacement.y() / pointDistance);
  }

  TransformationGroup group;
  if (horzOffset == 0 && vertOffset == 0) {
    return group;
  }

  const std::unordered_set<VizPolyhedron *> &selectedPolyhedra =
      m_polyhedron->coordinateSystem()->projection()->selectionManager()->selectedPolyhedra();

  for (VizPolyhedron *vp : selectedPolyhedra) {
    const std::vector<int> &beta = vp->occurrence()->betaVector();
    size_t horzDepth = vp->coordinateSystem()->horizontalDimensionIdx() == VizProperties::NO_DIMENSION ?
          VizProperties::NO_DIMENSION :
          vp->occurrence()->depth(vp->coordinateSystem()->horizontalDimensionIdx());
    size_t vertDepth = vp->coordinateSystem()->verticalDimensionIdx() == VizProperties::NO_DIMENSION ?
          VizProperties::NO_DIMENSION :
          vp->occurrence()->depth(vp->coordinateSystem()->verticalDimensionIdx());

    bool oneDimensional = vp->occurrence()->dimensionality() < vp->coordinateSystem()->verticalDimensionIdx();
    bool zeroDimensional = vp->occurrence()->dimensionality() < vp->coordinateSystem()->horizontalDimensionIdx();
    if (!zeroDimensional && horzOffset != 0) {
      Transformation transformation = Transformation::constantShift(beta, horzDepth, -horzOffset);
      group.transformations.push_back(transformation);
    }
    if (!oneDimensional && vertOffset != 0) {
      Transformation transformation = Transformation::constantShift(beta, vertDepth, -vertOffset);
      group.transformations.push_back(transformation);
    }
  }
  return group;
}

std::pair<TransformationGroup, TransformationGroup> VizManipulationShifting::computeShadowAndAnimation() {
  TransformationGroup shadowGroup = computeShadowAnimationInternal(false);
  TransformationGroup animationGroup = computeShadowAnimationInternal(true);
  return std::make_pair(shadowGroup, animationGroup);
}

double VizManipulationShifting::animationRatio() {
  return 0;
}
