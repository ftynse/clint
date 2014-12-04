#ifndef VIZPROJECTION_H
#define VIZPROJECTION_H

#include <QGraphicsScene>
#include <QGraphicsView>
#include <QObject>

#include <vector>

#include "vizcoordinatesystem.h"
#include "vizmanipulationmanager.h"
#include "vizproperties.h"
#include "vizselectionmanager.h"

class VizProjection : public QObject
{
  Q_OBJECT
public:
  VizProjection(int horizontalDimensionIdx, int verticalDimensionIdx, QObject *parent = 0);
  QWidget *widget() {
    return m_view;
  }

  void projectScop(ClintScop *vscop);

  /*inline*/ const VizProperties *vizProperties() const {
    return m_vizProperties;
  }

  /*inline*/ VizSelectionManager *selectionManager() const {
    return m_selectionManager;
  }

  /*inline*/ VizManipulationManager *manipulationManager() const {
    return m_manipulationManager;
  }

  void updateColumnHorizontalMinMax(VizCoordinateSystem *coordinateSystem, int minOffset, int maxOffset);
  void ensureFitsHorizontally(VizCoordinateSystem *coordinateSystem, int minimum, int maximum);
  void ensureFitsVertically(VizCoordinateSystem *coordinateSystem, int minimum, int maximum);


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

  private:
    IsCsAction m_action;
    VizCoordinateSystem *m_vcs = nullptr;
    size_t m_pile;   // in case InsertPile, insert before this index; if index >= size, insert after the last; in case InsertCS, index of the pile to insert to
    size_t m_coordinateSystem;

    friend class VizProjection;
  };

  VizProjection::IsCsResult isCoordinateSystem(QPointF point);
  VizCoordinateSystem *ensureCoordinateSystem(IsCsResult csAt, int dimensionality);
  VizCoordinateSystem *createCoordinateSystem(int dimensionality);

  void paintProjection(QPainter *painter) {
    m_scene->render(painter);
  }

  QSize projectionSize() const {
    return m_scene->itemsBoundingRect().size().toSize();
  }

signals:

public slots:
  void updateProjection();

private:
  QGraphicsView *m_view;
  QGraphicsScene *m_scene;
  VizProperties *m_vizProperties;

  // Outer index = column index;
  // Inner index = row index within the given column
  // Different columns may have different number of rows.
  std::vector<std::vector<VizCoordinateSystem *>> m_coordinateSystems;
  int m_horizontalDimensionIdx;
  int m_verticalDimensionIdx;

  VizSelectionManager *m_selectionManager;
  VizManipulationManager *m_manipulationManager;

  void appendCoordinateSystem(int dimensionality);
  void updateSceneLayout();
};

#endif // VIZPROJECTION_H
