#ifndef VIZPROJECTION_H
#define VIZPROJECTION_H

#include <QGraphicsScene>
#include <QGraphicsView>
#include <QObject>

#include <vector>

#include "vizcoordinatesystem.h"

class VizProjection : public QObject
{
  Q_OBJECT
public:
  VizProjection(int horizontalDimensionIdx, int verticalDimensionIdx, QObject *parent = 0);
  QWidget *widget() {
    return m_view;
  }

  void projectScop(ClintScop *vscop);
signals:

public slots:

private:
  QGraphicsView *m_view;
  QGraphicsScene *m_scene;

  // Outer index = column index;
  // Inner index = row index within the given column
  // Different columns may have different number of rows.
  std::vector<std::vector<VizCoordinateSystem *>> m_coordinateSystems;
  int m_horizontalDimensionIdx;
  int m_verticalDimensionIdx;

  void createCoordinateSystem(int dimensionality);
  void updateSceneLayout();
};

#endif // VIZPROJECTION_H
