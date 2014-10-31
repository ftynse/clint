#ifndef CLINTDEPENDENCE_H
#define CLINTDEPENDENCE_H

#include <QObject>
#include "oslutils.h"

class ClintStmtOccurrence;

class ClintDependence : public QObject {
  Q_OBJECT
public:
  explicit ClintDependence(osl_dependence_p dependence, ClintStmtOccurrence *source,
                           ClintStmtOccurrence *target, QObject *parent = nullptr);

  std::vector<std::vector<int>> projectOn(int horizontalDimIdx, int verticalDimIdx);

  int sourceDimensionality() const {
    return m_dependence->source_nb_output_dims_domain;
  }

  int targetDimensionality() const {
    return m_dependence->target_nb_output_dims_domain;
  }

  ClintStmtOccurrence *source() const {
    return m_source;
  }

  ClintStmtOccurrence *target() const {
    return m_target;
  }

signals:

public slots:

private:
  osl_dependence_p m_dependence;

  ClintStmtOccurrence *m_source;
  ClintStmtOccurrence *m_target;
};

#endif // CLINTDEPENDENCE_H
