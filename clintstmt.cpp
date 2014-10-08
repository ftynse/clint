#include "enumerator.h"
#include "macros.h"
#include "oslutils.h"
#include "clintstmt.h"
#include "clintstmtoccurrence.h"

#include <algorithm>
#include <map>
#include <utility>

ClintStmt::ClintStmt(osl_statement_p stmt, ClintScop *parent) :
  QObject(parent), m_scop(parent), m_statement(stmt) {

  // FIXME: redoing iteration over scattering union parts
  oslListForeach(stmt->scattering, [this](osl_relation_p scattering) {
    std::vector<int> betaVector = betaExtract(scattering);
    if (m_occurrences.count(betaVector) == 0) {
      ClintStmtOccurrence *vso = new ClintStmtOccurrence(m_statement, betaVector, this);
      m_occurrences[betaVector] = vso;
    }
  });
}

std::vector<ClintStmtOccurrence *> ClintStmt::occurences() const {
  std::vector<ClintStmtOccurrence *> result;
  for (auto occurence : m_occurrences) {
    result.push_back(occurence.second);
  }
  std::sort(std::begin(result), std::end(result), VizStmtOccurrencePtrComparator());
  return std::move(result);
}
