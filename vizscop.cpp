#include "macros.h"
#include "oslutils.h"
#include "vizscop.h"
#include "vizstatement.h"

VizScop::VizScop(osl_scop_p scop, VizProgram *parent) :
  QObject(parent), m_scop_part(scop), program_(parent) {
  m_betaMap = oslBetaMap(scop);
  oslListForeach(scop->statement, [this](osl_statement_p stmt) {
    VizStatement *vizStmt = new VizStatement(stmt, this);
    oslListForeach(stmt->scattering, [this,vizStmt](osl_relation_p scatter) {
      m_vizBetaMap[betaExtract(scatter)] = vizStmt;
    });
  });

  // FIXME: hardcoded parameter value
  m_fixedContext = oslRelationFixAllParameters(m_scop_part->context, 4);
}
