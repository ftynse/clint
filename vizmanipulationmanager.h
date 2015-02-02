#ifndef VIZMANIPULATIONMANAGER_H
#define VIZMANIPULATIONMANAGER_H

#include <QObject>
#include <QPointF>

class VizPolyhedron;
class VizPoint;
class VizCoordinateSystem;

struct TransformationGroup;

class VizManipulationManager : public QObject {
  Q_OBJECT
public:
  explicit VizManipulationManager(QObject *parent = 0);

  void rearrangePiles2D(std::vector<int> &createdBeta, bool pileDeleted, TransformationGroup &group, int pileIdx, size_t pileNb);
  void rearrangeCSs2D(int coordinateSystemIdx, bool csDeleted, std::vector<int> &createdBeta, size_t pileSize, TransformationGroup &group);

  enum class Dir {
    LEFT,
    RIGHT,
    UP,
    DOWN
  };

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

  void polyhedronAboutToResize(VizPolyhedron *polyhedron, Dir direction);
  void polyhedronResizing(QPointF displacement);
  void polyhedronHasResized(VizPolyhedron *polyhedron);

private:
  VizPolyhedron *m_polyhedron = nullptr;
  VizPoint *m_point = nullptr;
  VizCoordinateSystem *m_coordinateSystem = nullptr;

  int m_initCSHorizontalMin, m_initCSVerticalMin;

  int m_horzOffset, m_vertOffset;
  bool m_detached;
  bool m_firstMovement = false;
  Dir m_direction;

  enum {
    PT_NODETACH,
    PT_DETACH_HORIZONTAL,
    PT_DETACH_VERTICAL,
    PT_DETACHED_HORIZONTAL,
    PT_DETACHED_VERTICAL
  }    m_pointDetachState;
  int  m_pointDetachValue;
  bool m_pointDetachLast;

  void ensureTargetConsistency();
};

#endif // VIZMANIPULATIONMANAGER_H
