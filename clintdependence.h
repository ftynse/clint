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

signals:

public slots:

private:
  osl_dependence_p m_dependence;

  ClintStmtOccurrence *m_source;
  ClintStmtOccurrence *m_target;
};

#endif // CLINTDEPENDENCE_H
