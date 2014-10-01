#include "macros.h"
#include "oslutils.h"
#include "vizscop.h"
#include "vizstatement.h"

VizScop::VizScop(osl_scop_p scop, VizProgram *parent) :
  QObject(parent), m_scop_part(scop), program_(parent) {
  oslListForeach(scop->statement, [this](osl_statement_p stmt) {
    VizStatement *vizStmt = new VizStatement(stmt, this);
    oslListForeach(stmt->scattering, [this,vizStmt](osl_relation_p scatter) {
      m_vizBetaMap[betaExtract(scatter)] = vizStmt;
    });
  });

  // FIXME: hardcoded parameter value
  m_fixedContext = oslRelationFixAllParameters(m_scop_part->context, 4);
}

std::unordered_set<VizStatement *> VizScop::statements() const {
  std::unordered_set<VizStatement *> stmts;
  for (auto value : m_vizBetaMap)
    stmts.insert(value.second);
  return std::move(stmts);
}
