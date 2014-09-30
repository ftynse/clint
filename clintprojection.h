#ifndef CLINTPROJECTION_H
#define CLINTPROJECTION_H

#include <QGraphicsScene>
#include <QGraphicsView>
#include <QObject>

#include <vector>

#include "vizcoordinatesystem.h"

class ClintProjection : public QObject
{
  Q_OBJECT
public:
  ClintProjection(int horizontalDimensionIdx, int verticalDimensionIdx, QObject *parent = 0);
  QWidget *widget() {
    return m_view;
  }

  void buildFromScop__(osl_scop_p scop);
  void projectScop(VizScop *vscop);
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

  void createCoordinateSystem(const std::vector<int> &betaVector);
  void updateSceneLayout();
};

#endif // CLINTPROJECTION_H
