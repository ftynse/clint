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

private:
  VizPolyhedron *m_polyhedron = nullptr;
  VizPoint *m_point = nullptr;
  VizCoordinateSystem *m_coordinateSystem = nullptr;

  int m_initCSHorizontalMin, m_initCSVerticalMin;

  int m_horzOffset, m_vertOffset;
  bool m_detached;
  bool m_firstMovement = false;

  void ensureTargetConsistency();
};

#endif // VIZMANIPULATIONMANAGER_H
