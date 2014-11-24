#ifndef VIZSELECTIONMANAGER_H
#define VIZSELECTIONMANAGER_H

#include <QObject>
#include <unordered_set>

#include "macros.h"

class VizPolyhedron;
class VizPoint;
class VizCoordinateSystem;

class VizSelectionManager : public QObject {
  Q_OBJECT
public:
  explicit VizSelectionManager(QObject *parent = 0);

  enum class ConflictPolicy {
    KeepOld,
    ClearOld
  };

  enum class TypeConflictPolicy {
    KeepOld,
    AllowMix,
    ClearOld
  };

  const std::unordered_set<VizPolyhedron *> &selectedPolyhedra() const {
    return m_selectedPolyhedra;
  }

signals:
  void selectionChanged();

public slots:
  void polyhedronSelectionChanged(VizPolyhedron *polyhedron, bool selected);
  void pointSelectionChanged(VizPoint *point, bool selected);
  void coordinateSystemSelectionChanged(VizCoordinateSystem *coordinateSystem, bool selected);
  void clearSelection();

private:
  std::unordered_set<VizPolyhedron *>       m_selectedPolyhedra;
  std::unordered_set<VizPoint *>            m_selectedPoints;
  std::unordered_set<VizCoordinateSystem *> m_selectedCoordinateSystems;

  ConflictPolicy m_conflictPolicy = ConflictPolicy::ClearOld;
  TypeConflictPolicy m_typeConflictPolicy = TypeConflictPolicy::ClearOld;

  bool m_selectionBarrier = false;

  template <typename T>
  void clearSelection(std::unordered_set<T *> &selection) {
    for (T *element : selection) {
      element->setSelected(false);
    }
    selection.clear();
  }

  template <typename T>
  bool resolveConflict(std::unordered_set<T *> &selection, T *element) {
    switch (m_conflictPolicy) {
    case ConflictPolicy::ClearOld:
      clearSelection(selection);
      return true;
      break;
    case ConflictPolicy::KeepOld:
      return false;
      break;
    }
  }

  template <typename T>
  bool resolveTypeConflict(std::unordered_set<T *> &selection, T *element) {
    switch(m_typeConflictPolicy) {
    case TypeConflictPolicy::ClearOld:
      clearSelection();
      return true;
      break;
    case TypeConflictPolicy::AllowMix:
      return true;
      break;
    case TypeConflictPolicy::KeepOld:
      return false;
      break;
    }
  }
};

#endif // VIZSELECTIONMANAGER_H
