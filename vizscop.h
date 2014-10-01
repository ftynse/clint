#ifndef VIZSCOP_H
#define VIZSCOP_H

#include <QObject>

#include "vizprogram.h"

#include <map>
#include <unordered_set>
#include <vector>

class VizStatement;

class VizScop : public QObject
{
  Q_OBJECT
public:
  typedef std::map<std::vector<int>, VizStatement *> VizBetaMap;

  explicit VizScop(osl_scop_p scop, VizProgram *parent = nullptr);

  // Accessors
  VizProgram *program() const {
    return program_;
  }

  const VizBetaMap &vizBetaMap() const {
    return m_vizBetaMap;
  }

  osl_relation_p fixedContext() const {
    return m_fixedContext;
  }

  std::unordered_set<VizStatement *> statements() const;

signals:

public slots:

private:
  osl_scop_p m_scop_part;
  VizProgram *program_;
  osl_relation_p m_fixedContext;
//  std::vector<VizStatement *> statements_;
  // statements = unique values of m_vizBetaMap
  VizBetaMap m_vizBetaMap;
};

#endif // VIZSCOP_H
