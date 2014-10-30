#include "clintdependence.h"
#include "clintstmtoccurrence.h"

ClintDependence::ClintDependence(osl_dependence_p dependence,
                                 ClintStmtOccurrence *source,
                                 ClintStmtOccurrence *target,
                                 QObject *parent) :
  QObject(parent), m_dependence(dependence), m_source(source), m_target(target) {
}
