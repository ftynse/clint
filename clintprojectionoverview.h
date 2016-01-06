#ifndef CLINTPROJECTIONOVERVIEW_H
#define CLINTPROJECTIONOVERVIEW_H

#include <QWidget>

#include <map>
#include <utility>

#include "vizprojection.h"
#include "vizproperties.h"

class QGridLayout;
class QSvgGenerator;

class ClintProjectionOverview : public QWidget {
  Q_OBJECT
public:
  explicit ClintProjectionOverview(ClintScop *cscop, QWidget *parent = nullptr);

  void resetProjectionMatrix(ClintScop *cscop);
  VizProperties *vizProperties();
  void fillSvg(QSvgGenerator *generator);
  void reflectBetaTransformations(ClintScop *cscop, const TransformationGroup &group);

signals:
  void projectionSelected(int horizontalDim, int verticalDim);

public slots:
  void updateAllProjections();
  void updateProjection(int horizontalDim, int verticalDim);
  void updateRowColumn(int horizontalDim, int verticalDim);

private:
  std::map<std::pair<int, int>, VizProjection *> m_allProjections = std::map<std::pair<int, int>, VizProjection *>();
  QGridLayout *m_layout = nullptr;
};

#endif // CLINTPROJECTIONOVERVIEW_H
