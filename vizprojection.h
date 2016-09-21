#ifndef VIZPROJECTION_H
#define VIZPROJECTION_H

#include <QGraphicsScene>
#include <QGraphicsView>
#include <QObject>

#include <vector>
#include <unordered_map>

#include "projectionview.h"
#include "vizcoordinatesystem.h"
#include "vizmanipulationmanager.h"
#include "vizproperties.h"
#include "vizselectionmanager.h"

class VizProjection : public QObject {
  Q_OBJECT
public:
  VizProjection(int horizontalDimensionIdx, int verticalDimensionIdx, ClintScop *vscop, QObject *parent = nullptr);
  QWidget *widget() {
    return m_view;
  }

  /*inline*/ VizProperties *vizProperties() const {
    return m_vizProperties;
  }

  /*inline*/ VizSelectionManager *selectionManager() const {
    return m_selectionManager;
  }

  /*inline*/ VizManipulationManager *manipulationManager() const {
    return m_manipulationManager;
  }

  /*inline*/ ClintScop *scop() const {
    return m_scop;
  }

  void updateColumnHorizontalMinMax(VizCoordinateSystem *coordinateSystem, int minOffset, int maxOffset);
  void ensureFitsHorizontally(VizCoordinateSystem *coordinateSystem, int minimum, int maximum);
  void ensureFitsVertically(VizCoordinateSystem *coordinateSystem, int minimum, int maximum);

  int horizontalDimensionIdx() const {
    return m_horizontalDimensionIdx;
  }

  int verticalDimensionIdx() const {
    return m_verticalDimensionIdx;
  }

  enum class IsCsAction { Found, InsertPile, InsertCS };

  class IsCsResult {
  public:
    IsCsAction action() const {
      return m_action;
    }
    size_t pileIdx() const {
      return m_pile;
    }
    size_t coordinateSystemIdx() const {
      return m_coordinateSystem;
    }
    VizCoordinateSystem *coordinateSystem() const {
      return m_vcs;
    }

    void setFound(VizCoordinateSystem *cs) {
      if (m_action == IsCsAction::InsertPile) {
        m_coordinateSystem = 0;
      }
      m_action = IsCsAction::Found;
      m_vcs = cs;
    }

    bool operator ==(const IsCsResult &r) {
      return m_pile == r.m_pile &&
             m_coordinateSystem == r.m_coordinateSystem &&
             m_vcs == r.m_vcs &&
             m_action == r.m_action;
    }

    bool operator !=(const IsCsResult &r) {
      return !operator ==(r);
    }

  private:
    size_t m_pile;   // in case InsertPile, insert before this index; if index >= size, insert after the last; in case InsertCS, index of the pile to insert to
    size_t m_coordinateSystem;
    IsCsAction m_action;
    VizCoordinateSystem *m_vcs = nullptr;

    friend class VizProjection;
  };

  VizProjection::IsCsResult isCoordinateSystem(QPointF point, VizPolyhedron *polyhedron);
  VizCoordinateSystem *ensureCoordinateSystem(IsCsResult &csAt, int dimensionality);
  VizCoordinateSystem *createCoordinateSystem(int dimensionality);
  void deleteCoordinateSystem(VizCoordinateSystem *vcs);
  VizCoordinateSystem *firstNonEmptyCoordinateSystem(size_t pileIdx, VizPolyhedron *except = nullptr) const;

  void paintProjection(QPainter *painter) {
    m_scene->render(painter);
  }

  QSize projectionSize() const {
    return m_scene->itemsBoundingRect().size().toSize();
  }

  std::pair<size_t, size_t> csIndices(VizCoordinateSystem *vcs) const;
  size_t pileCSNumber(size_t pileIndex) const {
    CLINT_ASSERT(pileIndex < m_coordinateSystems.size(), "Pile index out of range");
    return m_coordinateSystems.at(pileIndex).size();
  }
  size_t pileNumber() const {
    return m_coordinateSystems.size();
  }

  VizCoordinateSystem *insertPile(IsCsResult csAt, int dimensionality);
  VizCoordinateSystem *insertCs(IsCsResult csAt, int dimensionality);
  void updateOuterDependences();
  void updateInnerDependences();
  void updateInternalDependences();

  void setViewActive(bool active) {
    if (m_view)
      m_view->setActive(active);
  }

  bool isViewActive() const {
    return m_view && m_view->isActive();
  }

  void skipNextBetaGroup(int groupSize) {
    m_skipBetaTransformations += groupSize;
  }

  VizPolyhedron *polyhedron(ClintStmtOccurrence *occurrence) const;
  void reflectBetaTransformation(const Transformation &transformation);

  void showInsertionShadow(IsCsResult r);
  IsCsResult emptyIsCsResult() const;

signals:
  void selected(int horizontal, int vertical);

public slots:
  void updateProjection();
  void selectProjection();
  void updateSceneLayout();
  void updateSceneLayoutAnimated();
  void finalizeOccurrenceChange();

private:
  ProjectionView *m_view         = nullptr;
  QGraphicsScene *m_scene        = nullptr;
  VizProperties *m_vizProperties = nullptr;
  ClintScop *m_scop              = nullptr;

  // Outer index = column index;
  // Inner index = row index within the given column
  // Different columns may have different number of rows.
  std::vector<std::vector<VizCoordinateSystem *>> m_coordinateSystems;
  int m_horizontalDimensionIdx;
  int m_verticalDimensionIdx;
  QGraphicsItem *m_insertionShadow = nullptr;
  IsCsResult m_previousIsCsResult = emptyIsCsResult();

  VizSelectionManager *m_selectionManager;
  VizManipulationManager *m_manipulationManager;

  int m_skipBetaTransformations = 0;

  // When "reflecting" beta transformations triggered elsewhere, we want to have animated
  // movement of the coordinate systems.  This flag overrides the behavior of updateSceneLayout
  // for the next call to actually call updateSceneLayoutAnimated.
  bool m_nextSceneUpdateAnimated = false;

  void projectScop();
  void appendCoordinateSystem(int dimensionality);
  std::unordered_map<VizCoordinateSystem *, std::pair<double, double>> recomputeCoordinateSystemPositions();
  VizCoordinateSystem *createCoordinateSystem(VizCoordinateSystem *oldVCS);
  IsCsResult findCoordinateSystem(QPointF point);
  void correctIsCs(IsCsResult &result, VizPolyhedron *polyhedron);
};

template <typename Element>
std::vector<Element> reflectReorder(const std::vector<Element> &container,
                                    std::function<std::vector<int> (const Element &)> betaExtractor,
                                    Transformation transformation) {
  int dimension = transformation.target().size();
  std::multimap<int, size_t> ordering;
  bool foundPrefix = false;
  for (size_t idx = 0; idx < container.size(); ++idx) {
    std::vector<int> beta = betaExtractor(container[idx]);
    if (!BetaUtility::isPrefix(transformation.target(), beta)) {
      // Current ordering should reflect correct lexicographical order of beta-vectors.
      // Therefore, before the target prefix found, all beta-vectors preceed it, and after it
      // was fully processed, all beta-vectors succeed it (no interleaving possible).
      // We keep the order of these vectors by collecting their previous indices with at
      // keys -1 and INT_MAX respectively.
      ordering.insert(std::make_pair(foundPrefix ? INT_MAX : -1, idx));
    } else {
      foundPrefix = true;
      CLINT_ASSERT(beta.size() > dimension, "dimension/prefix mismatch");
      // Keep the order of beta-vectors with identical beta-values.
      ordering.insert(std::make_pair(beta.at(dimension), idx));
    }
  }

  // Insertion order is preserved allowing us to simulate moving a group of
  // sequentially placed piles with the same beta-value (multimap key).
  // First, put all preceeding piles (not subject to current reorder depth),
  // then follow the reordering list and, finally, put the remainings piles.
  std::vector<Element> updatedContainer;
  updatedContainer.reserve(container.size());
  std::vector<int> order = transformation.order();
  order.insert(std::begin(order), -1);
  order.push_back(INT_MAX);
  updatedContainer.resize(container.size());
  size_t index = 0;
  for (int key : order) {
    std::multimap<int, size_t>::iterator it, eit;
    std::tie(it, eit) = ordering.equal_range(key);
    for ( ; it != eit; ++it) {
      updatedContainer[it->second] = container[index++];
    }
  }

  return updatedContainer;
}

#endif // VIZPROJECTION_H
