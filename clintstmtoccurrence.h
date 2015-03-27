#ifndef CLINTSTMTOCCURRENCE_H
#define CLINTSTMTOCCURRENCE_H

#include "clintstmt.h"

#include <osl/relation.h>
#include <osl/statement.h>

#include <QObject>

#include <initializer_list>
#include <set>
#include <unordered_map>
#include <vector>

class ClintStmtOccurrence : public QObject {
  Q_OBJECT
public:
  ClintStmtOccurrence(osl_statement_p stmt,
                    const std::vector<int> &betaVector,
                    ClintStmt *parent = 0);

  friend bool operator < (const ClintStmtOccurrence &lhs, const ClintStmtOccurrence &rhs);
  friend bool operator ==(const ClintStmtOccurrence &lhs, const ClintStmtOccurrence &rhs);

  int ignoreTilingDim(int dimension) const;
  std::vector<std::vector<int>> projectOn(int horizontalDimIdx, int verticalDimIdx) const;

  int dimensionality() const {
    return static_cast<int>(m_betaVector.size())
        - std::count_if(std::begin(m_tilingDimensions), std::end(m_tilingDimensions), [](int i) { return i % 2 == 0;})
        - 1;
  }

  ClintStmt *statement() const {
    return m_statement;
  }

  ClintProgram *program() const {
    return m_statement->program();
  }

  ClintScop *scop() const {
    return m_statement->scop();
  }

  const std::vector<int> &betaVector() const {
    return m_betaVector;
  }

  const std::set<int> &tilingDimensions() const {
    return m_tilingDimensions;
  }

  void tile(int dimensionIdx, unsigned tileSize) {
    m_tileSize = tileSize;
    if (tileSize == 0) {
      m_tilingDimensions.erase(2 * dimensionIdx);
      m_tilingDimensions.erase(2 *dimensionIdx + 1);
    } else {
      m_tilingDimensions.insert(2 * dimensionIdx);
      m_tilingDimensions.insert(2 *dimensionIdx + 1);
    }
  }

  bool isTiled() const {
    return m_tileSize != 0;
  }

  unsigned tileSize() const {
    return m_tileSize;
  }

  std::vector<int> untiledBetaVector() const;

  int minimumValue(int dimIdx) const;
  int maximumValue(int dimIdx) const;

  void resetOccurrence(osl_statement_p stmt, const std::vector<int> &betaVector);
  void resetBetaVector(const std::vector<int> &betaVector);

  enum class Bound {
    LOWER,
    UPPER
  };

  std::vector<int> findBoundlikeForm(Bound bound, int dimIdx, int constValue);

signals:
  void pointsChanged();
  void betaChanged();

public slots:

private:
  std::vector<osl_relation_p> m_oslScatterings;
  osl_statement_p m_oslStatement;
  std::vector<int> m_betaVector;
  std::set<int> m_tilingDimensions;
  ClintStmt *m_statement;
  unsigned m_tileSize = 0;

  // Caches for min/max.
  mutable std::unordered_map<int, int> m_cachedDimMins;
  mutable std::unordered_map<int, int> m_cachedDimMaxs;

  void computeMinMax(const std::vector<std::vector<int>> &points,
                     int horizontalDimIdx, int verticalDimIdx) const;
  std::vector<int> makeBoundlikeForm(Bound bound, int dimIdx, int constValue, int constantBoundaryPart, const std::vector<int> &parameters, const std::vector<int> &parameterValues);
};

struct VizStmtOccurrencePtrComparator {
  bool operator () (const ClintStmtOccurrence *lhs, const ClintStmtOccurrence *rhs) {
    return *lhs < *rhs;
  }
};

#endif // CLINTSTMTOCCURRENCE_H
