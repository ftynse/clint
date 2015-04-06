#ifndef CLINTSTMT_H
#define CLINTSTMT_H

#include <QObject>

#include "clintprogram.h"
#include "clintscop.h"

#include <map>
#include <vector>

class ClintStmtOccurrence;

class ClintStmt : public QObject {
  Q_OBJECT
public:
  explicit ClintStmt(osl_statement_p stmt, ClintScop *parent = nullptr);

  ClintScop *scop() const {
    return m_scop;
  }

  ClintProgram *program() const {
    return m_scop->program();
  }

  std::vector<ClintStmtOccurrence *> occurrences() const;

  ClintStmtOccurrence *occurrence(const std::vector<int> &beta) const {
    auto iterator = m_occurrences.find(beta);
    if (iterator == std::end(m_occurrences))
      return nullptr;
    return iterator->second;
  }

  ClintStmtOccurrence *makeOccurrence(osl_statement_p stmt, const std::vector<int> &beta);
  ClintStmtOccurrence *splitOccurrence(ClintStmtOccurrence *occurrence, osl_statement_p stmt, const std::vector<int> &beta);

  std::string dimensionName(int dimension) const {
    CLINT_ASSERT(dimension >= 0, "Dimension index should be positive");
    if (dimension < m_dimensionNames.size())
      return m_dimensionNames[dimension];
//    CLINT_UNREACHABLE;
    return std::string("??");
  }

  void updateBetas(const std::map<std::vector<int>, std::vector<int>> &mapping);

signals:

public slots:

private:
  ClintScop *m_scop;
  osl_statement_p m_statement;
  std::map<std::vector<int>, ClintStmtOccurrence *> m_occurrences;
  std::vector<std::string> m_dimensionNames;
};

#endif // CLINTSTMT_H
