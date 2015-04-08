#include "macros.h"
#include "oslutils.h"
#include "clintdependence.h"
#include "clintscop.h"
#include "clintstmt.h"
#include "clintstmtoccurrence.h"

#include <exception>
#include <map>
#include <set>
#include <sstream>
#include <tuple>
#include <utility>

#include <QColor>
#include <QString>
#include "vizproperties.h"

void ClintScop::clearDependences() {
  for (auto it : m_dependenceMap) {
    it.second->setParent(nullptr);
    delete it.second;
  }
  m_dependenceMap.clear();
  m_internalDeps.clear();
}

void ClintScop::processDependenceMap(const DependenceMap &dependenceMap,
                                     const std::vector<osl_dependence_p> &violated) {
  for (auto element : dependenceMap) {
    std::pair<std::vector<int>, std::vector<int>> betas;
    betas = element.first;
    osl_dependence_p dep = element.second;
    bool isViolated = false;
    if (std::find(std::begin(violated), std::end(violated), dep) != std::end(violated)) {
      isViolated = true;
    }
    ClintStmtOccurrence *source = mappedOccurrence(betas.first);
    ClintStmtOccurrence *target = mappedOccurrence(betas.second);

    ClintDependence *clintDep = new ClintDependence(dep, source, target, isViolated, this);

    m_dependenceMap.emplace(std::make_pair(m_betaMapper->map(betas.first).first,
                                           m_betaMapper->map(betas.second).first),
                            clintDep);
    if (source == target) {
      m_internalDeps.emplace(source, clintDep);
    }
  }
}

void ClintScop::createDependences(osl_scop_p scop) {
  clearDependences();

  DependenceMap dependenceMap = oslDependenceMap(scop);
  std::vector<osl_dependence_p> emptyVector;
  processDependenceMap(dependenceMap, emptyVector);
}

void ClintScop::updateDependences(osl_scop_p transformed) {
  clearDependences();

  std::vector<osl_dependence_p> violated;
  DependenceMap dependenceMap = oslDependenceMapViolations(m_scopPart, transformed, &violated);

  processDependenceMap(dependenceMap, violated);
}

static inline void replaceNewlinesHtml(std::string &str) {
  size_t pos = 0;
  while ((pos = str.find('\n', pos)) != std::string::npos) {
    str.replace(pos, 1, "\n<br/>");
    pos += 6;
  }
}

ClintScop::ClintScop(osl_scop_p scop, int parameterValue, char *originalCode, ClintProgram *parent) :
  QObject(parent), m_scopPart(scop), m_program(parent), m_parameterValue(parameterValue) {
  oslListForeach(scop->statement, [this](osl_statement_p stmt) {
    ClintStmt *vizStmt = new ClintStmt(stmt, this);
    oslListForeach(stmt->scattering, [this,vizStmt](osl_relation_p scatter) {
      std::vector<int> beta = betaExtract(scatter);
      CLINT_ASSERT(m_vizBetaMap.find(beta) == std::end(m_vizBetaMap),
                   "Multiple scheduling union parts cannot have the same beta-vector");
      m_vizBetaMap[beta] = vizStmt;
    });
  });

  m_fixedContext = oslRelationFixAllParameters(m_scopPart->context, parameterValue);

  m_transformer = new ClayTransformer;
  m_scriptGenerator = new ClayScriptGenerator(m_scriptStream);
  m_betaMapper = new ClayBetaMapper(this);

  createDependences(scop);

  m_generatedCode = oslToCCode(m_scopPart);
  m_currentScript = (char *) malloc(sizeof(char));
  m_currentScript[0] = '\0';

  if (originalCode == nullptr) {
    m_originalCode = strdup(m_generatedCode);
    updateGeneratedHtml(m_scopPart, m_originalHtml);
  } else {
    m_originalCode = strdup(originalCode);
    m_originalHtml = std::string(escapeHtml(originalCode));

    replaceNewlinesHtml(m_originalHtml);
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

  replaceNewlinesHtml(string);

  delete props;
}

// This functions assumes betas have been remapped already.
void ClintScop::resetOccurrences(osl_scop_p transformed) {
  oslListForeachSingle(transformed->statement, [this](osl_statement_p stmt) {
    oslListForeachSingle(stmt->scattering, [this,stmt](osl_relation_p scattering) {
      std::vector<int> beta = betaExtract(scattering);
      ClintStmtOccurrence *occ = occurrence(beta);
      CLINT_ASSERT(occ, "No occurrence corresond to the beta-vector");
      occ->resetOccurrence(stmt, beta);
    });
  });
}

void ClintScop::executeTransformationSequence() {
  // XXX: speed/memory+code_quality tradeoff
  // ClintScop stores the original (non-transfomed) scop and we perform the entire transformation sequence after each action
  // However, it may store the current transformed scop along with the original (original is still needed for, e.g., undo)
  // and perform only the transformations from the last group that is identifiable by m_groupsExecuted.
  osl_scop_p transformed = osl_scop_clone(m_scopPart);
  size_t groupsExecuted = 0;
  for (const TransformationGroup &group : m_transformationSeq.groups) {
    for (const Transformation &transformation : group.transformations) {
      m_transformer->apply(transformed, transformation);

      if (groupsExecuted >= m_groupsExecuted) {
        // Create the occurrence to reflect the ISS result.
        if (transformation.kind() == Transformation::Kind::IndexSetSplitting) {
          std::vector<int> loopBeta = transformation.target();
          loopBeta.push_back(0); // FIXME: this assumes the loop being issed has only one statement (and makes assumptions on beta structure and transformation)
          ClintStmtOccurrence *occ = occurrence(loopBeta);
          loopBeta.back() = 1;
          occ->statement()->splitOccurrence(occ, nullptr, loopBeta); // XXX: nullptr statement was not checked and will fail.  fix ClintStmtOccurrence::resetOccurrence and ::projectOn (if called)
          m_vizBetaMap[loopBeta] = occ->statement();                 // The subsequent call to resetOccurrences will replace the statement anyway
        }
      }
    }
    ++groupsExecuted;
  }
  resetOccurrences(transformed);
  m_groupsExecuted = groupsExecuted;

  updateDependences(transformed);
  updateGeneratedHtml(transformed, m_generatedHtml);

  emit transformExecuted();
  bool dimensionNbChanged =
      std::find_if(std::begin(m_transformationSeq.groups), std::end(m_transformationSeq.groups),
                   [] (const TransformationGroup &group) {
    return std::find_if(std::begin(group.transformations), std::end(group.transformations),
                        [] (const Transformation &transformation) {
      return transformation.kind() == Transformation::Kind::Tile;
    }) != std::end(group.transformations);
  }) != std::end(m_transformationSeq.groups);
  if (dimensionNbChanged) {
    emit dimensionalityChanged();
  }
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

ClintStmtOccurrence *ClintScop::mappedOccurrence(const std::vector<int> &beta) const {
  std::vector<int> mappedBeta;
  int result;
  std::tie(mappedBeta, result) = m_betaMapper->map(beta);
  if (result != ClayBetaMapper::SUCCESS)
    return nullptr;
  return occurrence(mappedBeta);
}

std::vector<int> ClintScop::untiledBetaVector(const std::vector<int> &beta) const {
  ClintStmtOccurrence *o = occurrence(beta);
  if (o == nullptr)
    throw std::invalid_argument("Could not find an occurrence with the given beta");
  return o->untiledBetaVector();
}

const std::set<int> &ClintScop::tilingDimensions(const std::vector<int> &beta) const {
  ClintStmtOccurrence *o = occurrence(beta);
  if (o == nullptr)
    throw std::invalid_argument("Could not find an occurrence with the given beta");
  return o->tilingDimensions();
}

void ClintScop::remapBetas(const TransformationGroup &group) {
  // m_betaMapper keeps the mapping from the original beta-vectors to the current ones,
  // but we cannot find get a ClintStmtOccurrence by original beta-vector if their beta-vectors
  // were changed during transformation: some of them may have been created by ISS thus making
  // one original beta-vector correspond to multiple statements.  Thus we create another
  // temporary mapper to map from current beta-vectors to the new ones as well as we update the
  // m_betaMapper to keep dependency maps consistent.
  ClayBetaMapper2 *mapper2 = new ClayBetaMapper2(this);
  mapper2->apply(nullptr, group);
  //m_betaMapper2->apply(nullptr, group);
  m_betaMapper->apply(nullptr, group);
  mapper2->dump(std::cerr);

  std::map<std::vector<int>, std::vector<int>> mapping;
  for (ClintStmt *stmt : statements()) {
    for (ClintStmtOccurrence *occurrence : stmt->occurrences()) {
      std::set<std::vector<int>> mappedBetas =
          mapper2->forwardMap(occurrence->betaVector());
      CLINT_ASSERT(mappedBetas.size() == 1,
                   "Beta remapping is only possible for one-to-one mapping."); // Did you forget to add/remove occurrences before calling it?
      mapping[occurrence->betaVector()] = *mappedBetas.begin();
      occurrence->resetBetaVector(*mappedBetas.begin());
    }
  }

  delete mapper2;
  updateBetas(mapping);
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

// TODO: use this function to implement internalDependences; remove m_internalDeps.
void ClintScop::forwardDependencesBetween(ClintStmtOccurrence *occ1,
                                          ClintStmtOccurrence *occ2,
                                          std::unordered_set<ClintDependence *> &result) const {
  auto range = m_dependenceMap.equal_range(std::make_pair(occ1->betaVector(), occ2->betaVector())); // FIXME: no beta vectors here...!
  for (auto element = range.first; element != range.second; element++) {
    result.emplace(element->second);
  }
}

std::unordered_set<ClintDependence *>
ClintScop::dependencesBetween(ClintStmtOccurrence *occ1, ClintStmtOccurrence *occ2) const {
  std::unordered_set<ClintDependence *> result;
  forwardDependencesBetween(occ1, occ2, result);
  forwardDependencesBetween(occ2, occ1, result);
  return std::move(result);
}

void ClintScop::undoTransformation() {
  if (!hasUndo())
    return;
  m_undoneTransformationSeq.groups.push_back(m_transformationSeq.groups.back());
  m_transformationSeq.groups.erase(std::end(m_transformationSeq.groups) - 1);
  --m_groupsExecuted;
//  executeTransformationSequence();
}

void ClintScop::redoTransformation() {
  if (!hasRedo())
    return;
  m_transformationSeq.groups.push_back(m_undoneTransformationSeq.groups.back());
  m_undoneTransformationSeq.groups.erase(std::end(m_undoneTransformationSeq.groups) - 1);
  ++m_groupsExecuted;
//  executeTransformationSequence();
}

void ClintScop::clearRedo() {
  m_undoneTransformationSeq.groups.clear();
}

int ClintScop::dimensionality() {
  int dimensions = 0;
  for (ClintStmt *stmt : statements()) {
    for (ClintStmtOccurrence *occ : stmt->occurrences()) {
      dimensions = std::max(dimensions, occ->dimensionality());
    }
  }
  return dimensions;
}
