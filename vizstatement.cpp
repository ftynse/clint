#include "enumerator.h"
#include "macros.h"
#include "oslutils.h"
#include "vizstatement.h"


VizStatement::VizStatement(osl_statement_p stmt, VizScop *parent) :
  QObject(parent), scop_(parent) {

  osl_relation_p applied = oslApplyScattering(stmt);
  osl_relation_p appliedContextualized = oslRelationWithContext(applied, nullptr); //TODO: context
}
