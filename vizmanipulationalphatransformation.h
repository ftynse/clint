#ifndef VIZMANIPULATIONALPHATRANSFORMATION_H
#define VIZMANIPULATIONALPHATRANSFORMATION_H

#include "vizpolyhedron.h"
#include <boost/optional.hpp>
#include <utility>

class VizManipulationAlphaTransformation {
public:
  virtual void postAnimationCreate() = 0;
  virtual void postShadowCreate() = 0;
  virtual void correctUntilValid(TransformationGroup &group,
                                 boost::optional<std::pair<TransformationGroup, TransformationGroup>> &replacedGroup) = 0;
  virtual std::pair<TransformationGroup, TransformationGroup> computeShadowAndAnimation() = 0;
  virtual double animationRatio() = 0;

  virtual void setDisplacement(QPointF displacement) ;
  virtual QPointF displacement() const;

  virtual ~VizManipulationAlphaTransformation();

protected:
  QPointF m_displacement;
};

class VizManipulationSkewing : public VizManipulationAlphaTransformation {
public:
  VizManipulationSkewing(VizPolyhedron *polyhedron, int corner);

  virtual void postAnimationCreate() override;
  virtual void postShadowCreate() override;
  virtual void correctUntilValid(TransformationGroup &group,
                                 boost::optional<std::pair<TransformationGroup, TransformationGroup>> &replacedGroup) override;
  virtual std::pair<TransformationGroup, TransformationGroup> computeShadowAndAnimation() override;

  virtual double animationRatio() override;

  enum Corner {
    C_BOTTOM = 0x1, // TOP  = ~BOTTOM
    C_RIGHT  = 0x2, // LEFT = ~RIGHT
  };
  enum {
    C_LEFT = 0x0
  };
  enum {
    C_TOP = 0x0
  };

private:
  VizPolyhedron *m_polyhedron;
  int m_corner;
  QPointF m_previousDisplacement, m_displacementAtStart;

  QPointF m_startPosition, m_targetPosition;

  TransformationGroup computeShadowAnimationInternal(bool isAnimation);
};

class VizManipulationShifting : public VizManipulationAlphaTransformation {
public:
  explicit VizManipulationShifting(VizPolyhedron *polyhedron);
  virtual void postAnimationCreate() override;
  virtual void postShadowCreate() override;
  virtual void correctUntilValid(TransformationGroup &group,
                                 boost::optional<std::pair<TransformationGroup, TransformationGroup>> &replacedGroup) override;
  virtual std::pair<TransformationGroup, TransformationGroup> computeShadowAndAnimation() override;
  virtual double animationRatio() override;

private:
  VizPolyhedron *m_polyhedron;

  TransformationGroup computeShadowAnimationInternal(bool isAnimation);
};

#endif // VIZMANIPULATIONALPHATRANSFORMATION_H
