#ifndef VIZSTMTOCCURRENCE_H
#define VIZSTMTOCCURRENCE_H

#include <osl/relation.h>

#include <QObject>

#include <vector>

class VizStatement;

class VizStmtOccurrence : public QObject {
  Q_OBJECT
public:
  VizStmtOccurrence(osl_statement_p stmt,
                    const std::vector<int> &betaVector,
                    VizStatement *parent = 0);

  friend bool operator < (const VizStmtOccurrence &lhs, const VizStmtOccurrence &rhs);
  friend bool operator ==(const VizStmtOccurrence &lhs, const VizStmtOccurrence &rhs);

signals:

public slots:

private:
  std::vector<osl_relation_p> m_oslScatterings;
  std::vector<int> m_betaVector;
  VizStatement *m_statement;
};

#endif // VIZSTMTOCCURRENCE_H
