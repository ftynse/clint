#ifndef VIZSTMTOCCURRENCE_H
#define VIZSTMTOCCURRENCE_H

#include "vizstatement.h"

#include <osl/relation.h>
#include <osl/statement.h>

#include <QObject>

#include <unordered_map>
#include <vector>

class VizStmtOccurrence : public QObject {
  Q_OBJECT
public:
  VizStmtOccurrence(osl_statement_p stmt,
                    const std::vector<int> &betaVector,
                    VizStatement *parent = 0);

  friend bool operator < (const VizStmtOccurrence &lhs, const VizStmtOccurrence &rhs);
  friend bool operator ==(const VizStmtOccurrence &lhs, const VizStmtOccurrence &rhs);

  int firstDifferentDimension(const VizStmtOccurrence &that) {
    // With beta-vectors for statements, we cannot have a match that is not equality,
    // i.e. we cannot have simultaneously [1] and [1,3] as beta-vectors for statements.
    CLINT_ASSERT(!std::equal(std::begin(m_betaVector),
                             std::begin(m_betaVector) + std::min(m_betaVector.size(), that.m_betaVector.size()),
                             std::begin(that.m_betaVector)),
                 "One statement occurence corresponds to the beta-prefix (loop)");
    auto mismatchIterators =
        std::mismatch(std::begin(m_betaVector), std::end(m_betaVector),
        std::begin(that.m_betaVector), std::end(that.m_betaVector));
    int difference = mismatchIterators.first - std::begin(m_betaVector);
    return difference;
  }

  std::vector<std::vector<int>> projectOn(int horizontalDimIdx, int verticalDimIdx) const;

  int dimensionality() const {
    return static_cast<int>(m_betaVector.size() - 1);
  }

  VizStatement *statement() const {
    return m_statement;
  }

  VizProgram *program() const {
    return m_statement->program();
  }

  VizScop *scop() const {
    return m_statement->scop();
  }

  int minimumValue(int dimIdx) const;
  int maximumValue(int dimIdx) const;

signals:

public slots:

private:
  std::vector<osl_relation_p> m_oslScatterings;
  osl_statement_p m_oslStatement;
  std::vector<int> m_betaVector;
  VizStatement *m_statement;

  // Caches for min/max.
  mutable std::unordered_map<int, int> m_cachedDimMins;
  mutable std::unordered_map<int, int> m_cachedDimMaxs;

  void computeMinMax(const std::vector<std::vector<int>> &points,
                     int horizontalDimIdx, int verticalDimIdx) const;
};

struct VizStmtOccurrencePtrComparator {
  bool operator () (const VizStmtOccurrence *lhs, const VizStmtOccurrence *rhs) {
    return *lhs < *rhs;
  }
};

#endif // VIZSTMTOCCURRENCE_H
