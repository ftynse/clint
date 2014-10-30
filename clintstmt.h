#ifndef CLINTSTMT_H
#define CLINTSTMT_H

#include <QObject>

#include <clintprogram.h>
#include <clintscop.h>

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

  std::vector<ClintStmtOccurrence *> occurences() const;

  ClintStmtOccurrence *occurrence(const std::vector<int> &beta) const {
    auto iterator = m_occurrences.find(beta);
    if (iterator == std::end(m_occurrences))
      return nullptr;
    return iterator->second;
  }
signals:

public slots:

private:
  ClintScop *m_scop;
  osl_statement_p m_statement;
  std::map<std::vector<int>, ClintStmtOccurrence *> m_occurrences;
};

#endif // CLINTSTMT_H
