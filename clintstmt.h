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

  std::vector<std::vector<int>> projectOn(int horizontalDimIdx, int verticalDimIdx, const std::vector<int> &betaVector) const;

  std::vector<ClintStmtOccurrence *> occurences() const;
signals:

public slots:

private:
  ClintScop *m_scop;
  osl_statement_p m_statement;
  std::map<std::vector<int>, ClintStmtOccurrence *> m_occurrences;
};

#endif // CLINTSTMT_H
