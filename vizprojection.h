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

  void createCoordinateSystem(int dimensionality);
  void updateSceneLayout();
};

#endif // VIZPROJECTION_H
