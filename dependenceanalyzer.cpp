#include "dependenceanalyzer.h"
#include "macros.h"
#include "oslutils.h"

#include <unordered_set>

DependenceAnalyzer::DependenceAnalyzer() {
}

DependenceAnalyzer::~DependenceAnalyzer() {
}

CandlAnalyzer::CandlAnalyzer() {
}

CandlAnalyzer::~CandlAnalyzer() {
}


osl_dependence_p CandlAnalyzer::scopDependences(osl_scop_p scop) {
  candl_scop_usr_init(scop);
  osl_dependence_p dependences = candl_dependence(scop, m_candlOptions);
  if (dependences)
    candl_dependence_init_fields(scop, dependences);
  candl_scop_usr_cleanup(scop);
  return dependences;
}

std::pair<candl_violation_p, osl_dependence_p> CandlAnalyzer::scopViolations(osl_scop_p original, osl_scop_p transformed) {
  osl_dependence_p dependences;
  candl_violation_p violations = candl_violation(original, transformed, &dependences, m_candlOptions);
  return std::make_pair(violations, dependences);
}

DependenceAnalyzer::DependenceMap CandlAnalyzer::constructDependenceMap(osl_dependence_p dependence) {
  DependenceMap dependenceMap;
  oslListForeachSingle(dependence, [&dependenceMap](osl_dependence_p dep) {
    CLINT_ASSERT(dep->stmt_source_ptr != nullptr,
                 "Source statement pointer in the osl dependence must be defined");
    CLINT_ASSERT(dep->stmt_target_ptr != nullptr,
                 "Target statement pointer in the osl dependence must be defined");

    std::vector<int> sourceBeta = betaExtract(dep->stmt_source_ptr->scattering);
    std::vector<int> targetBeta = betaExtract(dep->stmt_target_ptr->scattering);
    dep->stmt_source_ptr = nullptr;   // Set up nulls intentionally, the data will be deleted.
    dep->stmt_target_ptr = nullptr;
    dependenceMap.emplace(std::make_pair(sourceBeta, targetBeta),
                          std::make_pair(dep, false));
  });
  return std::move(dependenceMap);
}

DependenceAnalyzer::DependenceMap CandlAnalyzer::analyze(osl_scop_p original, osl_scop_p transformed) {
  osl_dependence_p dependences;
  std::unordered_set<osl_dependence_p> violatedDependences;
  if (transformed != nullptr) {
    candl_violation_p violations;
    std::tie(violations, dependences) = scopViolations(original, transformed);
    oslListForeach(violations, [&violatedDependences](candl_violation_p violation){
      violatedDependences.insert(violation->dependence);
    });
  } else {
    dependences = scopDependences(original);
  }

  DependenceMap dependenceMap = constructDependenceMap(dependences);
  for (auto &el : dependenceMap) {
    if (violatedDependences.find(el.second.first) != violatedDependences.end()) {
      el.second.second = true;
    }
  }

  return dependenceMap;
}
