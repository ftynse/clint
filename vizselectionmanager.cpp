#include "vizcoordinatesystem.h"
#include "vizpoint.h"
#include "vizpolyhedron.h"
#include "vizselectionmanager.h"

#include <QtWidgets>
#include <QtGui>

#include <algorithm>

VizSelectionManager::VizSelectionManager(QObject *parent) :
  QObject(parent) {
}

void VizSelectionManager::polyhedronSelectionChanged(VizPolyhedron *polyhedron, bool selected) {
  if (!selected) {
    m_selectedPolyhedra.erase(polyhedron);
    return;
  }

  bool selectionAllowed = true;
  if (!m_selectedPoints.empty() || !m_selectedCoordinateSystems.empty()) {
    selectionAllowed = resolveTypeConflict(m_selectedPolyhedra, polyhedron);
  }
  if (!selectionAllowed)
    return;

  // Allow multiple selection of polygons from the same coordinate system only.
  bool sameCS = std::all_of(std::begin(m_selectedPolyhedra), std::end(m_selectedPolyhedra),
                            [polyhedron](VizPolyhedron *p) {
    return polyhedron->coordinateSystem() == p->coordinateSystem();
  });
  if (!sameCS) {
    selectionAllowed = resolveConflict(m_selectedPolyhedra, polyhedron);
  }
  if (!selectionAllowed)
    return;
  m_selectedPolyhedra.insert(polyhedron);
  emit selectionChanged();
}

// Point selection is not allowed
void VizSelectionManager::pointSelectionChanged(VizPoint *point, bool selected) {
  clearSelection(m_selectedPoints);
}

// Coordinate system selection is not allowed
void VizSelectionManager::coordinateSystemSelectionChanged(VizCoordinateSystem *coordinateSystem, bool selected) {
  clearSelection(m_selectedCoordinateSystems);
}

void VizSelectionManager::clearSelection() {
  clearSelection(m_selectedPolyhedra);
  clearSelection(m_selectedPoints);
  clearSelection(m_selectedCoordinateSystems);
}

