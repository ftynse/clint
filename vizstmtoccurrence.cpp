#include "oslutils.h"
#include "vizstatement.h"
#include "vizstmtoccurrence.h"

VizStmtOccurrence::VizStmtOccurrence(osl_statement_p stmt, const std::vector<int> &betaVector,
                                     VizStatement *parent) :
  QObject(parent), m_statement(parent) {
  oslListForeach(stmt->scattering, [this,&betaVector](osl_relation_p scattering) {
    if (betaExtract(scattering) == betaVector) {
      m_oslScatterings.push_back(scattering);
    }
  });
  CLINT_ASSERT(m_oslScatterings.size() != 0,
               "Trying to create an occurrence for the inexistent beta-vector");

  m_betaVector.reserve(betaVector.size());
  std::copy(std::begin(betaVector), std::end(betaVector), std::back_inserter(m_betaVector));
}

bool operator < (const VizStmtOccurrence &lhs, const VizStmtOccurrence &rhs) {
  return lhs.m_betaVector < rhs.m_betaVector;
}

bool operator ==(const VizStmtOccurrence &lhs, const VizStmtOccurrence &rhs) {
  return lhs.m_betaVector == rhs.m_betaVector;
}
