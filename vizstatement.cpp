#include "enumerator.h"
#include "macros.h"
#include "oslutils.h"
#include "vizstatement.h"
#include "vizstmtoccurrence.h"

#include <algorithm>
#include <map>
#include <utility>

VizStatement::VizStatement(osl_statement_p stmt, VizScop *parent) :
  QObject(parent), m_scop(parent), m_statement(stmt) {

  // FIXME: redoing iteration over scattering union parts
  oslListForeach(stmt->scattering, [this](osl_relation_p scattering) {
    std::vector<int> betaVector = betaExtract(scattering);
    if (m_occurrences.count(betaVector) == 0) {
      VizStmtOccurrence *vso = new VizStmtOccurrence(m_statement, betaVector, this);
      m_occurrences[betaVector] = vso;
    }
  });
}

std::vector<std::vector<int>> VizStatement::projectOn(int horizontalDimIdx,
                                                      int verticalDimIdx,
                                                      const std::vector<int> &betaVector) const {
}

std::vector<VizStmtOccurrence *> VizStatement::occurences() const {
  std::vector<VizStmtOccurrence *> result;
  for (auto occurence : m_occurrences) {
    result.push_back(occurence.second);
  }
  std::sort(std::begin(result), std::end(result), VizStmtOccurrencePtrComparator());
  return std::move(result);
}
