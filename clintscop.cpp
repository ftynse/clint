#include "macros.h"
#include "oslutils.h"
#include "clintdependence.h"
#include "clintscop.h"
#include "clintstmt.h"
#include "clintstmtoccurrence.h"

#include <tuple>
#include <utility>
#include <set>
#include <map>
#include <sstream>

#include <QColor>
#include <QString>
#include "vizproperties.h"

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

ClintScop::ClintScop(osl_scop_p scop, char *originalCode, ClintProgram *parent) :
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
  m_fixedContext = oslRelationFixAllParameters(m_scopPart->context, 6);

  createDependences(scop);

  m_transformer = new ClayTransformer;
  m_scriptGenerator = new ClayScriptGenerator(m_scriptStream);
  m_betaMapper = new ClayBetaMapper(this);

  m_generatedCode = oslToCCode(m_scopPart);
  m_currentScript = (char *) malloc(sizeof(char));
  m_currentScript[0] = '\0';

  if (originalCode == nullptr) {
    m_originalCode = strdup(m_generatedCode);
    updateGeneratedHtml(m_scopPart, m_originalHtml);
  } else {
    m_originalCode = strdup(originalCode);
    m_originalHtml = std::string(originalCode);
  }
}

ClintScop::~ClintScop() {
  delete m_transformer;
  delete m_scriptGenerator;
  delete m_betaMapper;

  free(m_generatedCode);
  free(m_currentScript);
  free(m_originalCode);
}

osl_scop_p ClintScop::appliedScop() {
  osl_scop_p scop = osl_scop_clone(m_scopPart);
  m_transformer->apply(scop, m_transformationSeq);
  return scop;
}

inline std::string rgbColorText(QColor clr) {
  char buffer[16];
  snprintf(buffer, 16, "%d,%d,%d", clr.red(), clr.green(), clr.blue());
  return std::string(buffer);
}

void ClintScop::updateGeneratedHtml(osl_scop_p transformedScop, std::string &string) {
  std::map<std::pair<int, int>, std::vector<int>> betasAtPos;

  std::multimap<std::vector<int>, std::pair<int, int>> positions = stmtPositionsInHtml(transformedScop);
  m_betaMapper->apply(m_scopPart, m_transformationSeq);
  for (auto it : positions) {
    int result;
    std::vector<int> reverseBeta;
    std::tie(reverseBeta, result) = m_betaMapper->reverseMap(it.first);
    if (result == ClayBetaMapper::SUCCESS) {
      betasAtPos.emplace(it.second, reverseBeta);
    } else {
      qDebug() << "no reverse for" << QVector<int>::fromStdVector(it.first) << result;
    }
  }

  if (m_generatedCode != nullptr)
    free(m_generatedCode);
  m_generatedCode = oslToCCode(transformedScop);
  if (m_currentScript != nullptr)
    free(m_currentScript);
  m_scriptGenerator->apply(m_scopPart, m_transformationSeq);
  m_currentScript = strdup(m_scriptStream.str().c_str());
  m_scriptStream.str(std::string());
  m_scriptStream.clear();

  char *generatedCode = escapeHtml(m_generatedCode);
  char *currentPtr = generatedCode;
  VizProperties *props = new VizProperties;  // FIXME: this does not belong to here, access it elsewhere
  std::stringstream html;
  for (auto it : betasAtPos) {
    if (it.first.first < (currentPtr - generatedCode)||
        it.first.first > it.first.second) {
      qDebug() << "Code positions are wrong";
      break;
    }
    html << std::string(currentPtr, it.first.first - (currentPtr - generatedCode));
    currentPtr = generatedCode + it.first.first;
    html << "<span style=\"color: rgb(" << rgbColorText(props->color(it.second)) << ");\">";
    html << std::string(currentPtr, (it.first.second - it.first.first));
    currentPtr = generatedCode + it.first.second;
    html << "</span>";
  }
  html << std::string(currentPtr);
  string.clear();
  string = html.str();

  size_t index = 0;
  while (true) {
    index = string.find("\n", index);
    if (index == string.npos)
      break;
    string.replace(index, 1, "\n<br/>");
    index += 6;
  }

  delete props;
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
      CLINT_ASSERT(occ != nullptr, "Occurrence corresponding to the beta-vector not found");
      occ->resetOccurrence(stmt, beta);
    });
  });

  updateGeneratedHtml(transformedScop, m_generatedHtml);

  emit transformExecuted();
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

// old->new
void ClintScop::updateBetas(std::map<std::vector<int>, std::vector<int>> &mapping) {
  // Update beta map.
  VizBetaMap newBetas;
  std::set<std::vector<int>> betaKeysToRemove;
  for (auto it : mapping) {
    newBetas.insert(std::make_pair(it.second, m_vizBetaMap[it.first]));
    betaKeysToRemove.insert(it.first);
  }
  for (auto it : betaKeysToRemove) {
    m_vizBetaMap.erase(it);
  }
  for (auto it : newBetas) {
    m_vizBetaMap.insert(it);
  }

  // Update statements.
  for (auto it : statements()) {
    it->updateBetas(mapping);
  }

  // Update dependence map.
  ClintDependenceMap newDeps;
  std::set<std::pair<std::vector<int>, std::vector<int> > > depKeysToRemove;
  for (auto it : m_dependenceMap) {
    bool firstBetaRemapped = mapping.count(it.first.first) == 1;
    bool secondBetaRemapped = mapping.count(it.first.second) == 1;
    if (firstBetaRemapped && secondBetaRemapped) {
      newDeps.insert(std::make_pair(
                       std::make_pair(mapping[it.first.first], mapping[it.first.second]),
                       it.second));
    } else if (firstBetaRemapped) {
      newDeps.insert(std::make_pair(
                       std::make_pair(mapping[it.first.first], it.first.second),
                       it.second));
    } else if (secondBetaRemapped) {
      newDeps.insert(std::make_pair(
                       std::make_pair(it.first.first, mapping[it.first.second]),
                       it.second));
    }
    if (firstBetaRemapped || secondBetaRemapped) {
      depKeysToRemove.insert(it.first);
    }
  }
  for (auto it : depKeysToRemove) {
    m_dependenceMap.erase(it);
  }
  for (auto it : newDeps) {
    m_dependenceMap.insert(it);
  }
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

void ClintScop::undoTransformation() {
  if (!hasUndo())
    return;
  m_undoneTransformationSeq.groups.push_back(m_transformationSeq.groups.back());
  m_transformationSeq.groups.erase(std::end(m_transformationSeq.groups) - 1);
//  executeTransformationSequence();
}

void ClintScop::redoTransformation() {
  if (!hasRedo())
    return;
  m_transformationSeq.groups.push_back(m_undoneTransformationSeq.groups.back());
  m_undoneTransformationSeq.groups.erase(std::end(m_undoneTransformationSeq.groups) - 1);
//  executeTransformationSequence();
}

void ClintScop::clearRedo() {
  m_undoneTransformationSeq.groups.clear();
}
