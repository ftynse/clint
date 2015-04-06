#ifndef CLINTSCOP_H
#define CLINTSCOP_H

#include <QObject>

#include <map>
#include <set>
#include <sstream>
#include <unordered_set>
#include <vector>

#include "clintprogram.h"
#include "transformation.h"
#include "transformer.h"

class ClintDependence;
class ClintStmt;
class ClintStmtOccurrence;

class ClintScop : public QObject {
  Q_OBJECT
public:
  typedef std::map<std::vector<int>, ClintStmt *> VizBetaMap;
  typedef std::multimap<std::pair<std::vector<int>, std::vector<int>>, ClintDependence *> ClintDependenceMap;
//  typedef std::multimap<std::pair<ClintStmtOccurrence *, ClintStmtOccurrence *>, ClintDependence *> ClintDependenceMap;
  typedef std::multimap<ClintStmtOccurrence *, ClintDependence *> ClintOccurrenceDeps;

  explicit ClintScop(osl_scop_p scop, int parameterValue, char *originalCode = nullptr, ClintProgram *parent = nullptr);
  ~ClintScop();

  // Accessors
  ClintProgram *program() const {
    return m_program;
  }

  const VizBetaMap &vizBetaMap() const {
    return m_vizBetaMap;
  }

  osl_relation_p fixedContext() const {
    return m_fixedContext;
  }

  std::unordered_set<ClintStmt *> statements() const;

  ClintStmt *statement(const std::vector<int> &beta) const {
    auto iterator = m_vizBetaMap.find(beta);
    if (iterator == std::end(m_vizBetaMap))
      return nullptr;
    return iterator->second;
  }

  void transform(const Transformation &t) {
    TransformationGroup tg;
    tg.transformations.push_back(t);
    transform(tg);
  }

  void transform(const TransformationGroup &tg) {
    m_transformationSeq.groups.push_back(tg);
    for (const Transformation &transformation : tg.transformations) {
      if (transformation.kind() == Transformation::Kind::Fuse ||
          transformation.kind() == Transformation::Kind::Split ||
          transformation.kind() == Transformation::Kind::Reorder ||
          transformation.kind() == Transformation::Kind::Tile) {
        remapBetas(tg);
        break;
      }
    }
  }

  void executeTransformationSequence();
  void remapBetas(const TransformationGroup &tg);

  ClintStmtOccurrence *occurrence(const std::vector<int> &beta) const;
  ClintStmtOccurrence *mappedOccurrence(const std::vector<int> &beta) const;
  std::unordered_set<ClintDependence *> internalDependences(ClintStmtOccurrence *occurrence) const;
  std::unordered_set<ClintDependence *> dependencesBetween(ClintStmtOccurrence *occ1, ClintStmtOccurrence *occ2) const;
  std::vector<int> untiledBetaVector(const std::vector<int> &beta) const;
  const std::set<int> &tilingDimensions(const std::vector<int> &beta) const;

  void updateBetas(std::map<std::vector<int>, std::vector<int> > &mapping);

  osl_scop_p appliedScop();
  void swapBetaMapper(ClintScop *scop) {
    std::swap(m_betaMapper, scop->m_betaMapper);
  }

  void setScopSilent(osl_scop_p scop) {
    updateGeneratedHtml(m_scopPart, m_generatedHtml);
    m_scopPart = scop;
    m_scriptGenerator->apply(scop, m_transformationSeq);
    if (m_currentScript)
      free(m_currentScript);
    m_currentScript = strdup(m_scriptStream.str().c_str());
    m_scriptStream.str(std::string());
    m_scriptStream.clear();
    updateGeneratedHtml(m_scopPart, m_originalHtml);
  }
  void updateCode() {
    updateGeneratedHtml(m_scopPart, m_generatedHtml);
  }

  osl_scop_p scopPart() const {
    return m_scopPart;
  }
  const TransformationSequence &transformationSequence() const {
    return m_transformationSeq;
  }
  const TransformationSequence &redoSequence() const {
    return m_undoneTransformationSeq;
  }
  void resetRedoSequence(const TransformationSequence &seq) {
    m_undoneTransformationSeq = seq;
  }

  const char *generatedCode() {
    return m_generatedCode;
  }

  const char *originalCode() {
    return m_originalCode;
  }

  const char *currentScript() {
    return m_currentScript;
  }

  const char *generatedHtml() const {
    return m_generatedHtml.c_str();
  }

  const char *originalHtml() const {
    return m_originalHtml.c_str();
  }

  bool hasUndo() const {
    return m_transformationSeq.groups.size() > 0;
  }

  bool hasRedo() const {
    return m_undoneTransformationSeq.groups.size() > 0;
  }

  std::vector<int> parameterValues() const {
    std::vector<int> parameters(m_scopPart->context->nb_parameters, m_parameterValue);
    return std::move(parameters);
  }

  int dimensionality();

signals:
  void transformExecuted();
  void dimensionalityChanged();

public slots:
  void undoTransformation();
  void redoTransformation();
  void clearRedo();

private:
  void updateGeneratedHtml(osl_scop_p transformedScop, std::string &string);
  void forwardDependencesBetween(ClintStmtOccurrence *occ1, ClintStmtOccurrence *occ2,
                                 std::unordered_set<ClintDependence *> &result) const;
  void clearDependences();
  void processDependenceMap(const DependenceMap &dependenceMap,
                            const std::vector<osl_dependence_p> &violated);
  void createDependences(osl_scop_p scop);
  void updateDependences(osl_scop_p transformed);
  void resetOccurrences(osl_scop_p transformed);

  osl_scop_p m_scopPart;
  ClintProgram *m_program;
  int m_parameterValue;
  osl_relation_p m_fixedContext;
//  std::vector<VizStatement *> statements_;
  // statements = unique values of m_vizBetaMap
  VizBetaMap m_vizBetaMap;
  ClintDependenceMap m_dependenceMap;
  ClintOccurrenceDeps m_internalDeps;

  TransformationSequence m_transformationSeq;
  TransformationSequence m_undoneTransformationSeq;
  Transformer *m_transformer;
  Transformer *m_scriptGenerator;
  ClayBetaMapper *m_betaMapper;
  size_t m_groupsExecuted = 0;

  char *m_originalCode  = nullptr;
  char *m_generatedCode = nullptr;
  char *m_currentScript = nullptr;
  std::stringstream m_scriptStream;
  std::string m_originalHtml, m_generatedHtml;
};

#endif // CLINTSCOP_H
