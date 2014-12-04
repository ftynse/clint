#include "vizcoordinatesystem.h"
#include "vizmanipulationmanager.h"
#include "vizpoint.h"
#include "vizpolyhedron.h"
#include "vizproperties.h"
#include "vizprojection.h"
#include "vizselectionmanager.h"

#include <QtGui>
#include <QtWidgets>

VizManipulationManager::VizManipulationManager(QObject *parent) :
  QObject(parent) {
}

void VizManipulationManager::ensureTargetConsistency() {
  int sum = 0;
  sum += (m_polyhedron != nullptr);
  sum += (m_point != nullptr);
  sum += (m_coordinateSystem != nullptr);
  // Should not be possible unless we are in the multitouch environment.
  CLINT_ASSERT(sum == 1, "More than one active movable object manipulated at the same time");
}

void VizManipulationManager::polyhedronAboutToMove(VizPolyhedron *polyhedron) {
  CLINT_ASSERT(m_polyhedron == nullptr, "Active polyhedron is already being manipulated");
  m_polyhedron = polyhedron;
  ensureTargetConsistency();

  m_horzOffset = 0;
  m_vertOffset = 0;
}

void VizManipulationManager::polyhedronMoving(VizPolyhedron *polyhedron, QPointF displacement) {
  CLINT_ASSERT(polyhedron == m_polyhedron, "Another polyhedron is being manipulated");
  VizCoordinateSystem *cs = polyhedron->coordinateSystem();
  const VizProperties *properties = cs->projection()->vizProperties();
  double absoluteDistanceX = displacement.x() / properties->pointDistance();
  double absoluteDistanceY = -displacement.y() / properties->pointDistance(); // Inverted vertical axis
  int horzOffset = static_cast<int>(round(absoluteDistanceX));
  int vertOffset = static_cast<int>(round(absoluteDistanceY));
  if (horzOffset != m_horzOffset || vertOffset != m_vertOffset) {
    // Get selected objects.  (Should be in the same coordinate system.)
    const std::unordered_set<VizPolyhedron *> &selectedPolyhedra =
        cs->projection()->selectionManager()->selectedPolyhedra();
    CLINT_ASSERT(std::find(std::begin(selectedPolyhedra), std::end(selectedPolyhedra), polyhedron) != std::end(selectedPolyhedra),
                 "The active polyhedra is not selected");
    int phHmin = INT_MAX, phHmax = INT_MIN, phVmin = INT_MAX, phVmax = INT_MIN;
    for (VizPolyhedron *ph : selectedPolyhedra) {
      phHmin = std::min(phHmin, ph->localHorizontalMin());
      phHmax = std::max(phHmax, ph->localHorizontalMax());
      phVmin = std::min(phVmin, ph->localVerticalMin());
      phVmax = std::max(phVmax, ph->localVerticalMax());
    }

    if (horzOffset != m_horzOffset) {
      int hmin = phHmin + horzOffset;
      int hmax = phHmax + horzOffset;
      cs->projection()->ensureFitsHorizontally(cs, hmin, hmax);
      emit intentionMoveHorizontally(horzOffset - m_horzOffset);
      m_horzOffset = horzOffset;
    }

    if (vertOffset != m_vertOffset) {
      int vmin = phVmin + vertOffset;
      int vmax = phVmax + vertOffset;
      cs->projection()->ensureFitsVertically(cs, vmin, vmax);
      emit intentionMoveVertically(vertOffset - m_vertOffset);
      m_vertOffset = vertOffset;
    }
  }
}

void VizManipulationManager::polyhedronHasMoved(VizPolyhedron *polyhedron) {
  CLINT_ASSERT(m_polyhedron == polyhedron, "Signaled end of polyhedron movement that was never initiated");
  m_polyhedron = nullptr;
  TransformationGroup group;
  const std::unordered_set<VizPolyhedron *> &selectedPolyhedra =
      polyhedron->coordinateSystem()->projection()->selectionManager()->selectedPolyhedra();
  if (m_horzOffset != 0 || m_vertOffset != 0) {
    // TODO: move this code to transformation manager
    // we need transformation manager to keep three different views in sync
    CLINT_ASSERT(std::find(std::begin(selectedPolyhedra), std::end(selectedPolyhedra), polyhedron) != std::end(selectedPolyhedra),
                 "The active polyhedra is not selected");

    // TODO: vertical should be ignored (and movement forbidden) for 1D coordinate systems
    for (VizPolyhedron *vp : selectedPolyhedra) {
      const std::vector<int> beta = vp->occurrence()->betaVector();
      int horzDepth = vp->coordinateSystem()->horizontalDimensionIdx() + 1;
      int horzAmount = m_horzOffset;
      int vertDepth = vp->coordinateSystem()->verticalDimensionIdx() + 1;
      int vertAmount = m_vertOffset;
      if (m_horzOffset != 0) {
        Transformation transformation = Transformation::consantShift(beta, horzDepth, -horzAmount);
        group.transformations.push_back(transformation);
      }
      if (m_vertOffset != 0) {
        Transformation transformation = Transformation::consantShift(beta, vertDepth, -vertAmount);
        group.transformations.push_back(transformation);
      }
      CLINT_ASSERT(vp->scop() == polyhedron->scop(), "All statement occurrences in the transformation group must be in the same scop");
    }

    if (m_horzOffset != 0)
      emit movedHorizontally(m_horzOffset);
    if (m_vertOffset != 0)
      emit movedVertically(m_vertOffset);
  } else {
    for (VizPolyhedron *vp : selectedPolyhedra) {
      vp->coordinateSystem()->polyhedronUpdated(vp);
    }
  }

  if (!group.transformations.empty()) {
    polyhedron->scop()->transformed(group);
    polyhedron->scop()->executeTransformationSequence();
  }
}


void VizManipulationManager::polyhedronAboutToDetach(VizPolyhedron *polyhedron) {
  CLINT_ASSERT(m_polyhedron == nullptr, "Active polyhedron is already being manipulated");
  m_polyhedron = polyhedron;
  ensureTargetConsistency();

  CLINT_ASSERT(m_polyhedron->coordinateSystem()->coordinateSystemRect().contains(polyhedron->pos()),
               "Coordinate system rectangle does not contain the polyhedron");
  m_detached = false;

  QPointF position = polyhedron->coordinateSystem()->mapToParent(polyhedron->pos());
  VizProjection::IsCsResult r = polyhedron->coordinateSystem()->projection()->isCoordinateSystem(position);
  CLINT_ASSERT(r.action() == VizProjection::IsCsAction::Found && r.coordinateSystem() == polyhedron->coordinateSystem(),
               "Polyhedron position is not found in the coordinate system it actually belongs to");
}

void VizManipulationManager::polyhedronDetaching(QPointF position) {
  QRectF mainRect = m_polyhedron->coordinateSystem()->coordinateSystemRect();
  if (!mainRect.contains(position)) {
    if (!m_detached) {
      qDebug() << "detached";
    }
    m_detached = true;
  }
}

void VizManipulationManager::polyhedronHasDetached(VizPolyhedron *polyhedron) {
  CLINT_ASSERT(m_polyhedron == polyhedron, "Signaled end of polyhedron movement that was never initiated");
  m_polyhedron = nullptr;

  const std::unordered_set<VizPolyhedron *> &selectedPolyhedra =
      polyhedron->coordinateSystem()->projection()->selectionManager()->selectedPolyhedra();
  CLINT_ASSERT(std::find(std::begin(selectedPolyhedra), std::end(selectedPolyhedra), polyhedron) != std::end(selectedPolyhedra),
               "The active polyhedra is not selected");
  if (m_detached) {
    QPointF position = polyhedron->coordinateSystem()->mapToParent(polyhedron->pos());
    VizProjection::IsCsResult r = polyhedron->coordinateSystem()->projection()->isCoordinateSystem(position);
    int dimensionality = -1;
    for (VizPolyhedron *vp : selectedPolyhedra) {
      dimensionality = std::max(dimensionality, vp->occurrence()->dimensionality());
    }
    VizCoordinateSystem *cs = polyhedron->coordinateSystem()->projection()->ensureCoordinateSystem(r, dimensionality);
    for (VizPolyhedron *vp : selectedPolyhedra) {
      vp->reparent(cs);
    }
  }
}

