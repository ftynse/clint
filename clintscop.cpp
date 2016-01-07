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
#include "dependenceanalyzer.h"

void ClintScop::clearDependences() {
  for (auto it : m_dependenceMap) {
    it.second->setParent(nullptr);
    delete it.second;
  }
  m_dependenceMap.clear();
  m_internalDeps.clear();
}

void ClintScop::processDependenceMap(const DependenceAnalyzer::DependenceMap &dependenceMap) {
  for (auto element : dependenceMap) {
    std::pair<std::vector<int>, std::vector<int>> betas = element.first;
    osl_dependence_p dependence = element.second.first;
    bool isViolated = element.second.second;

    std::set<std::vector<int>> mappedSourceBetas = m_betaMapper->forwardMap(betas.first);
    std::set<std::vector<int>> mappedTargetBetas = m_betaMapper->forwardMap(betas.second);

    for (auto sourceBeta : mappedSourceBetas) {
      for (auto targetBeta : mappedTargetBetas) {
        ClintStmtOccurrence *source = occurrence(sourceBeta);
        ClintStmtOccurrence *target = occurrence(targetBeta);
        ClintDependence *clintDep = new ClintDependence(dependence, source, target, isViolated, this);
        m_dependenceMap.emplace(std::make_pair(sourceBeta, targetBeta), clintDep);
      }
    }
  }
}

void ClintScop::createDependences(osl_scop_p scop) {
  clearDependences();
  processDependenceMap(m_analyzer->analyze(scop));
}

void ClintScop::updateDependences(osl_scop_p transformed) {
  clearDependences();
  processDependenceMap(m_analyzer->analyze(m_scopPart, transformed));
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
  m_analyzer = new CandlAnalyzer;

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
  delete m_analyzer;

  free(m_generatedCode);
  free(m_currentScript);
  free(m_originalCode);

  appliedScopFlushCache();
}

osl_scop_p ClintScop::appliedScop() {
  if (m_appliedScopCache == nullptr) {
    m_appliedScopCache = osl_scop_clone(m_scopPart);
    m_transformer->apply(m_appliedScopCache, m_transformationSeq);
  }
  return m_appliedScopCache;
}

void ClintScop::appliedScopFlushCache() {
  if (m_appliedScopCache != nullptr) {
    osl_scop_free(m_appliedScopCache);
    m_appliedScopCache = nullptr;
  }
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
    betasAtPos.emplace(it.second, canonicalOriginalBetaVector(it.first));
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
        // Create the occurrence to reflect the ISS/Collapse result.
        // XXX: this assumes loop contains only one statement, and makes assumptions on beta-structure.
        // It is true for the transformation sequences created by VizManipulationManager.
        if (transformation.kind() == Transformation::Kind::IndexSetSplitting) {
          std::vector<int> loopBeta = transformation.target();
          loopBeta.push_back(0);
          ClintStmtOccurrence *occ = occurrence(loopBeta);
          loopBeta.back() = 1;
          occ->statement()->splitOccurrence(occ, loopBeta);
          m_vizBetaMap[loopBeta] = occ->statement();                 // The subsequent call to resetOccurrences will replace the statement anyway
        } else if (transformation.kind() == Transformation::Kind::Collapse) {
          std::vector<int> loopBeta = transformation.target();
          loopBeta.push_back(1);
          ClintStmtOccurrence *occ = occurrence(loopBeta);
          occ->statement()->removeOccurrence(occ);
          m_vizBetaMap.erase(loopBeta);
        }
      }
    }
    ++groupsExecuted;
  }
  resetOccurrences(transformed);
  m_groupsExecuted = groupsExecuted;

  updateDependences(transformed);
  updateGeneratedHtml(transformed, m_generatedHtml);
  appliedScopFlushCache();

  emit transformExecuted();
  bool dimensionNbChanged =
      std::find_if(std::begin(m_transformationSeq.groups), std::end(m_transformationSeq.groups),
                   [] (const TransformationGroup &group) {
    return std::find_if(std::begin(group.transformations), std::end(group.transformations),
                        [] (const Transformation &transformation) {
      return transformation.kind() == Transformation::Kind::Tile ||
             transformation.kind() == Transformation::Kind::Linearize ||
             transformation.kind() == Transformation::Kind::Embed ||
             transformation.kind() == Transformation::Kind::Unembed;
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

std::unordered_set<ClintStmtOccurrence *> ClintScop::occurrences(const std::vector<int> &betaPrefix) const {
  std::unordered_set<ClintStmtOccurrence *> found;
  for (ClintStmt *stmt : statements()) {
    for (ClintStmtOccurrence *occurrence : stmt->occurrences()) {
      if (BetaUtility::isPrefixOrEqual(betaPrefix, occurrence->betaVector())) {
        found.insert(occurrence);
      }
    }
  }
  return std::move(found);
}

int ClintScop::lastValueInLoop(const std::vector<int> &loopBeta) const {
  // Assuming m_vizBetaMap has all relevant beta (transformed).
  int value = -1;
  for (auto pair : m_vizBetaMap) {
    const std::vector<int> &beta = pair.first;
    if (BetaUtility::isPrefix(loopBeta, beta)) {
      value = std::max(value, beta[loopBeta.size()]);
    }
  }
  return value;
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

void ClintScop::remapWithTransformationGroup(size_t index) {
  const TransformationGroup &tg = m_transformationSeq.groups.at(index);

  bool createdComplementary = false;
  for (const Transformation &transformation : tg.transformations) {
    // Remap betas when needed.  FIXME: ClintScop should not know which transformation may modify betas
    // introduce bool Transformation::modifiesLoopStmtOrder() and use it.  Same for checking for ISS transformation.
    if (transformation.kind() == Transformation::Kind::Fuse ||
        transformation.kind() == Transformation::Kind::Split ||
        transformation.kind() == Transformation::Kind::Reorder ||
        transformation.kind() == Transformation::Kind::Tile ||
        transformation.kind() == Transformation::Kind::Linearize ||
        transformation.kind() == Transformation::Kind::Embed ||
        transformation.kind() == Transformation::Kind::Unembed) {
      emit groupAboutToExecute(index);
      remapBetas(tg);
      createdComplementary = true;
      break;
    }
    // XXX: needs rethinking
    // This weird move allows to workaround the 1-to-1 mapping condition imposed by the current implementation of remapBetas.
    // The problem is primarily caused by the fact of ClintStmtOccurrence creation in executeTransformationSequence (thus
    // apart from beta remapping) that happens in the undefined future; it is also not clear how to deal with multiple statements
    // being created by a transformation (which beta to assign to the remapped statement and which to the one being created;
    // is it important at all? occurrences will filter the scatterings they need, but not sure about dependence maps).
    // As long as VizManipulationManager deals with the association of VizPolyhedron to ClintStmtOccurrrence after it was created,
    // that part works fine.
    if (transformation.kind() == Transformation::Kind::IndexSetSplitting ||
        transformation.kind() == Transformation::Kind::Collapse) {
      m_betaMapper->apply(nullptr, tg);
      break;
    }
  }
  if (!createdComplementary)
    m_complementaryTransformationSeq.groups.push_back(TransformationGroup());
}


void ClintScop::remapBetasFull() {
  // Update all occurrences to their initial values.
  std::map<std::vector<int>, std::vector<int>> mapping;
  std::set<std::vector<int>> allOriginalBetas;
  for (ClintStmt *stmt : statements()) {
    for (ClintStmtOccurrence *occ : stmt->occurrences()) {
      const std::vector<int> &currentBeta = occ->betaVector();
      std::set<std::vector<int>> originalBetas = m_betaMapper->reverseMap(currentBeta);
      CLINT_ASSERT(originalBetas.size() != 0, "could not perform reverse mapping");

      // Recreate collapsed occurrences.
      for (int i = 1; i < originalBetas.size(); i++) {
        std::vector<int> temporaryBeta;
        temporaryBeta.push_back(-i);
        std::copy(std::begin(currentBeta), std::end(currentBeta), std::back_inserter(temporaryBeta));
        occ->statement()->splitOccurrence(occ, temporaryBeta);
        std::set<std::vector<int>>::iterator it = originalBetas.begin();
        std::advance(it, i);
        CLINT_ASSERT(allOriginalBetas.count(*it) == 0,
                     "Occurrence both iss-ed and collapsed");
        mapping.insert(std::make_pair(temporaryBeta, *it));
      }

      // Remove iss-ed occurrences.
      if (allOriginalBetas.count(*originalBetas.begin()) != 0) {
        occ->statement()->removeOccurrence(occ);
      } else {
        mapping.insert(std::make_pair(currentBeta, *originalBetas.begin()));
        allOriginalBetas.insert(*originalBetas.begin());
      }
    }
  }
  updateBetas(mapping);

  m_betaMapper->reset();

  m_complementaryTransformationSeq.groups.clear();
  for (size_t i = 0; i < m_transformationSeq.groups.size(); ++i) {
    remapWithTransformationGroup(i);
  }
}

void ClintScop::remapBetas(const TransformationGroup &group) {
  // m_betaMapper keeps the mapping from the original beta-vectors to the current ones,
  // but we cannot find get a ClintStmtOccurrence by original beta-vector if their beta-vectors
  // were changed during transformation: some of them may have been created by ISS thus making
  // one original beta-vector correspond to multiple statements.  Thus we create another
  // temporary mapper to map from current beta-vectors to the new ones as well as we update the
  // m_betaMapper to keep dependency maps consistent.
  ClayBetaMapper *mapper = new ClayBetaMapper(this);
  TransformationGroup complementary = mapper->apply(group);
  m_complementaryTransformationSeq.groups.push_back(complementary);
  m_betaMapper->apply(nullptr, group);

  std::map<std::vector<int>, std::vector<int>> mapping;
  for (ClintStmt *stmt : statements()) {
    for (ClintStmtOccurrence *occurrence : stmt->occurrences()) {
      std::set<std::vector<int>> mappedBetas =
          mapper->forwardMap(occurrence->betaVector());
      CLINT_ASSERT(mappedBetas.size() == 1,
                   "Beta remapping is only possible for one-to-one mapping."); // Did you forget to add/remove occurrences before calling it?
      mapping[occurrence->betaVector()] = *mappedBetas.begin();
      occurrence->resetBetaVector(*mappedBetas.begin());
    }
  }

  delete mapper;
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
  std::unordered_set<ClintDependence *> result;
  forwardDependencesBetween(occurrence, occurrence, result);
  return std::move(result);
}

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
//  m_transformationSeq.groups.erase(std::end(m_transformationSeq.groups) - 1);
//  --m_groupsExecuted;
//  remapBetasFull();
  if (m_complementaryTransformationSeq.groups.back().transformations.empty()) {
  m_transformationSeq.groups.erase(std::end(m_transformationSeq.groups) - 1, std::end(m_transformationSeq.groups));
  m_complementaryTransformationSeq.groups.erase(std::end(m_complementaryTransformationSeq.groups) - 1,
                                                std::end(m_complementaryTransformationSeq.groups));
  executeTransformationSequence();
  m_groupsExecuted -= 1;

  } else {
  transform(m_complementaryTransformationSeq.groups.back());
  executeTransformationSequence();
  m_transformationSeq.groups.erase(std::end(m_transformationSeq.groups) - 2, std::end(m_transformationSeq.groups));
  m_complementaryTransformationSeq.groups.erase(std::end(m_complementaryTransformationSeq.groups) - 2,
                                                std::end(m_complementaryTransformationSeq.groups));
  m_groupsExecuted -= 2;
  }
  // TODO: update visible sequence properly
}

void ClintScop::redoTransformation() {
  if (!hasRedo())
    return;
//  m_transformationSeq.groups.push_back(m_undoneTransformationSeq.groups.back());
  transform(m_undoneTransformationSeq.groups.back());
  m_undoneTransformationSeq.groups.erase(std::end(m_undoneTransformationSeq.groups) - 1);
  ++m_groupsExecuted;
//  remapBetasFull();
  executeTransformationSequence();
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

// There is no clear way how to "merge" colors from two statements.
// Just select the color of the first by default.
std::vector<int> ClintScop::canonicalOriginalBetaVector(const std::vector<int> &beta) const {
  std::set<std::vector<int>> betas = m_betaMapper->reverseMap(beta);
  CLINT_ASSERT(betas.size() != 0, "Occurrence does not have any original betas.");
  return *betas.begin();
}

void ClintScop::tile(const std::vector<int> &betaPrefix, int dimensionIdx, int tileSize) {
  for (ClintStmtOccurrence *occ : occurrences(betaPrefix)) {
    occ->tile(dimensionIdx, tileSize);
  }
}

void ClintScop::untile(const std::vector<int> &betaPrefix, int dimensionIdx) {
  for (ClintStmtOccurrence *occ : occurrences(betaPrefix)) {
    occ->untile(dimensionIdx);
  }
}
