#ifndef VIZMANIPULATIONMANAGER_H
#define VIZMANIPULATIONMANAGER_H

#include <QObject>
#include <QPointF>

#include <boost/optional.hpp>

#include "vizhandle.h"
#include "vizmanipulationalphatransformation.h"

class VizPolyhedron;
class VizPoint;
class VizCoordinateSystem;

struct TransformationGroup;

class ClintScop;

class VizManipulationManager : public QObject {
  Q_OBJECT
public:
  explicit VizManipulationManager(QObject *parent = 0);

  void rearrangePiles2D(std::vector<int> &createdBeta, bool pileDeleted, TransformationGroup &group, int pileIdx, size_t pileNb);
  void rearrangeCSs2D(int coordinateSystemIdx, bool csDeleted, std::vector<int> &createdBeta, size_t pileSize, TransformationGroup &group);

  enum Dir {
    LEFT,
    RIGHT,
    UP,
    DOWN
  };

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

  int rotationCase();
signals:
  void intentionMoveHorizontally(int amount);
  void intentionMoveVertically(int amount);
  void movedHorizontally(int amount);
  void movedVertically(int amount);

public slots:
  void polyhedronAboutToMove(VizPolyhedron *polyhedron);
  void polyhedronMoving(VizPolyhedron *polyhedron, QPointF displacement);
  void polyhedronHasMoved(VizPolyhedron *polyhedron);

  void polyhedronAboutToDetach(VizPolyhedron *polyhedron);
  void polyhedronDetaching(QPointF position);
  void polyhedronHasDetached(VizPolyhedron *polyhedron);

  void pointAboutToMove(VizPoint *point);
  void pointMoving(QPointF position);
  void pointHasMoved(VizPoint *point);

  void pointRightClicked(VizPoint *point);

  void polyhedronAboutToResize(VizPolyhedron *polyhedron, Dir direction);
  void polyhedronResizing(QPointF displacement);
  void polyhedronHasResized(VizPolyhedron *polyhedron);
  void polyhedronHasCreatedDimension(VizPolyhedron *polyhedron);

  void polyhedronAboutToSkew(VizPolyhedron *polyhedron, int corner);
  void polyhedronSkewing(QPointF displacement);
  void polyhedronHasSkewed(VizPolyhedron *polyhedron);

  void polyhedronAboutToRotate(VizPolyhedron *polyhedron, int corner);
  void polyhedronRotating(QPointF displacement);
  void polyhedronHasRotated(VizPolyhedron *polyhedron);

private:
  VizPolyhedron *m_polyhedron = nullptr;
  VizPoint *m_point = nullptr;
  VizCoordinateSystem *m_coordinateSystem = nullptr;
  TransformationGroup m_currentShadowGroup, m_currentAnimationGroup;
  boost::optional<std::pair<TransformationGroup, TransformationGroup>> m_replacedShadowGroup = boost::none;
  boost::optional<std::pair<TransformationGroup, TransformationGroup>> m_replacedAnimationGroup = boost::none;

  QPointF m_previousDisplacement;
  VizCoordinateSystem *m_betaTransformationTargetCS = nullptr;

  int m_horzOffset, m_vertOffset;
  bool m_detached;
  Dir m_direction;
  int m_corner;
  double m_rotationAngle;
  bool m_skewing = false;
  bool m_resizing = false;
  int m_creatingDimension = 0;

  VizManipulationAlphaTransformation *m_alphaTransformation;
  void startAlphaTransformation();
  void alphaTransformationProcess(QPointF displacement);
  void finalizeAlphaTransformation();

  enum {
    PT_NODETACH,
    PT_DETACH_HORIZONTAL,
    PT_DETACH_VERTICAL,
    PT_DETACHED_HORIZONTAL,
    PT_DETACHED_VERTICAL,
    PT_ATTACH,
    PT_ATTACHED
  }    m_pointDetachState;
  int  m_pointDetachValue;
  bool m_pointDetachLast;

  void ensureTargetConsistency();

  bool isFlagSet(int flags, int flag) {
    if (flag == 0) {
      return (~flags) & flag;
    }
    return flags & flag;
  }

  void remapBetas(TransformationGroup group, ClintScop *scop);
};

#endif // VIZMANIPULATIONMANAGER_H
