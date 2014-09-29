#include "macros.h"
#include "oslutils.h"
#include "vizscop.h"
#include "vizstatement.h"

VizScop::VizScop(osl_scop_p scop, VizProgram *parent) :
  QObject(parent), program_(parent) {
  osl_statement_p stmt;
  LL_FOREACH(stmt, scop->statement) {
    VizStatement *vizStmt = new VizStatement(stmt, this);
    statements_.push_back(vizStmt);
  }
}
