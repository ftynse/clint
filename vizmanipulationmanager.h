#ifndef VIZMANIPULATIONMANAGER_H
#define VIZMANIPULATIONMANAGER_H

#include <QObject>
#include <QPointF>

class VizPolyhedron;
class VizPoint;
class VizCoordinateSystem;

class VizManipulationManager : public QObject {
  Q_OBJECT
public:
  explicit VizManipulationManager(QObject *parent = 0);

signals:
  void intentionMoveHorizontally(int amount);
  void intentionMoveVertically(int amount);
  void movedHorizontally(int amount);
  void movedVertically(int amount);

public slots:
  void polyhedronAboutToMove(VizPolyhedron *polyhedron);
  void polyhedronMoving(VizPolyhedron *polyhedron, QPointF displacement);
  void polyhedronHasMoved(VizPolyhedron *polyhedron);

private:
  VizPolyhedron *m_polyhedron = nullptr;
  VizPoint *m_point = nullptr;
  VizCoordinateSystem *m_coordinateSystem = nullptr;

  int m_horzOffset, m_vertOffset;

  void ensureTargetConsistency();
};

#endif // VIZMANIPULATIONMANAGER_H
