#include "macros.h"
#include "oslutils.h"
#include "clintdependence.h"
#include "clintscop.h"
#include "clintstmt.h"
#include "clintstmtoccurrence.h"

#include <tuple>
#include <utility>

void ClintScop::createDependences(osl_scop_p scop) {
  DependenceMap dependenceMap = oslDependenceMap(scop);
  for (auto element : dependenceMap) {
    std::pair<std::vector<int>, std::vector<int>> betas;
    betas = element.first;
    osl_dependence_p dep = element.second;
    ClintStmtOccurrence *source = occurrence(betas.first);
    ClintStmtOccurrence *target = occurrence(betas.second);

    ClintDependence *clintDep = new ClintDependence(dep, source, target, this);
    m_dependenceMap.emplace(betas, clintDep);
    if (source == target) {
      m_internalDeps.emplace(source, clintDep);
    }
  }
}

ClintScop::ClintScop(osl_scop_p scop, ClintProgram *parent) :
  QObject(parent), m_scopPart(scop), m_program(parent) {
  oslListForeach(scop->statement, [this](osl_statement_p stmt) {
    ClintStmt *vizStmt = new ClintStmt(stmt, this);
    oslListForeach(stmt->scattering, [this,vizStmt](osl_relation_p scatter) {
      std::vector<int> beta = betaExtract(scatter);
      CLINT_ASSERT(m_vizBetaMap.find(beta) == std::end(m_vizBetaMap),
                   "Multiple scheduling union parts cannot have the same beta-vector");
      m_vizBetaMap[beta] = vizStmt;
    });
  });

  // FIXME: hardcoded parameter value
  m_fixedContext = oslRelationFixAllParameters(m_scopPart->context, 4);

  createDependences(scop);

  m_transformer = new ClayTransformer;
}

ClintScop::~ClintScop() {
  delete m_transformer;
}

void ClintScop::executeTransformationSequence() {
  osl_scop_p transformedScop = osl_scop_clone(m_scopPart);
  m_transformer->apply(transformedScop, m_transformationSeq);

  // TODO: load clay script from file as TransformationSequence
  // TODO(osl): create another extension to OSL that describes transformation groups

  // Since shift transformation does not affect betas, we can use them as statement identifiers
  // TODO(osl): maintain an ID for scattering union part throughout Clay transformations
  // and use them as identifiers, rather than betas
  oslListForeachSingle(transformedScop->statement, [this,&transformedScop](osl_statement_p stmt) {
    oslListForeachSingle(stmt->scattering, [this,&stmt](osl_relation_p scattering) {
      std::vector<int> beta = betaExtract(scattering);
      ClintStmtOccurrence *occ = occurrence(beta);
      occ->resetOccurrence(stmt, beta);
    });
  });
}

std::unordered_set<ClintStmt *> ClintScop::statements() const {
  std::unordered_set<ClintStmt *> stmts;
  for (auto value : m_vizBetaMap)
    stmts.insert(value.second);
  return std::move(stmts);
}

ClintStmtOccurrence *ClintScop::occurrence(const std::vector<int> &beta) const {
  ClintStmt *stmt = statement(beta);
  if (stmt == nullptr)
    return nullptr;
  return stmt->occurrence(beta);
}

std::unordered_set<ClintDependence *>
ClintScop::internalDependences(ClintStmtOccurrence *occurrence) const {
  auto range = m_internalDeps.equal_range(occurrence);
  std::unordered_set<ClintDependence *> result;
  for (auto element = range.first; element != range.second; element++) {
    result.emplace(element->second);
  }
  return std::move(result);
}
