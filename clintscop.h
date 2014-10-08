#ifndef CLINTSCOP_H
#define CLINTSCOP_H

#include <QObject>

#include "clintprogram.h"

#include <map>
#include <unordered_set>
#include <vector>

class ClintStmt;

class ClintScop : public QObject
{
  Q_OBJECT
public:
  typedef std::map<std::vector<int>, ClintStmt *> VizBetaMap;

  explicit ClintScop(osl_scop_p scop, ClintProgram *parent = nullptr);

  // Accessors
  ClintProgram *program() const {
    return program_;
  }

  const VizBetaMap &vizBetaMap() const {
    return m_vizBetaMap;
  }

  osl_relation_p fixedContext() const {
    return m_fixedContext;
  }

  std::unordered_set<ClintStmt *> statements() const;

signals:

public slots:

private:
  osl_scop_p m_scop_part;
  ClintProgram *program_;
  osl_relation_p m_fixedContext;
//  std::vector<VizStatement *> statements_;
  // statements = unique values of m_vizBetaMap
  VizBetaMap m_vizBetaMap;
};

#endif // CLINTSCOP_H
