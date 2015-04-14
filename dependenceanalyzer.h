#ifndef DEPENDENCEANALYZER_H
#define DEPENDENCEANALYZER_H

#include <candl/candl.h>
#include <osl/osl.h>
#include <osl/extensions/dependence.h>

#include <map>
#include <vector>

class DependenceAnalyzer {
public:
  typedef std::pair<osl_dependence_p, bool> Dependence;
  typedef std::multimap<std::pair<std::vector<int>, std::vector<int>>,
                        Dependence> DependenceMap;

  DependenceAnalyzer();
  virtual ~DependenceAnalyzer();

  virtual DependenceMap analyze(osl_scop_p original,
                                osl_scop_p transformed = nullptr) = 0;
};

class CandlRAII {
public:
  CandlRAII() {
    m_options = candl_options_malloc();
    m_options->fullcheck = 1;
  }

  ~CandlRAII() {
    candl_options_free(m_options);
  }

  operator candl_options_p () {
    return m_options;
  }

private:
  candl_options_p m_options;
};

class CandlAnalyzer : public DependenceAnalyzer{
public:
  CandlAnalyzer();
  virtual ~CandlAnalyzer();

  DependenceMap analyze(osl_scop_p original, osl_scop_p transformed = nullptr);

private:
  CandlRAII m_candlOptions;

  DependenceMap constructDependenceMap(osl_dependence_p dependence);
  std::pair<candl_violation_p, osl_dependence_p> scopViolations(osl_scop_p original, osl_scop_p transformed);
  osl_dependence_p scopDependences(osl_scop_p scop);

};

#endif // DEPENDENCEANALYZER_H
