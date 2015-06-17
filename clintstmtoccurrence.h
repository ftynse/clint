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

  ClintStmtOccurrence *split(const std::vector<int> &betaVector);

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

  void tile(int dimensionIdx, unsigned tileSize);

  bool isTiled() const {
    return !m_tilingDimensions.empty();
  }

  bool isTiled(int dimension) const {
    dimension = ignoreTilingDim(dimension);
    return m_tilingDimensions.find(dimension - 2) != std::end(m_tilingDimensions);
  }

  unsigned tileSize(int dim) const {
    if (m_tilingDimensions.find(dim) == std::end(m_tilingDimensions)) {
      return 0;
    }
    CLINT_ASSERT(m_tileSizes.find(dim) != std::end(m_tileSizes),
                 "Dimension is tiled, but no tile size associated."); // Probably, a beta dimension.
    return m_tileSizes.at(dim);
  }

  /**
   * Get the 1-based depth that may be used for transformation.
   * Takes tiling dimensions into account.
   * \param[in]  dimension 0-based visible dimension index.
   * \returns    1-based depth for Clay transformations.
   */
  int depth(int dimension) const {
    CLINT_ASSERT(dimension >= 0,
                 "Visible dimension must be positive");
    int scatDimension = 2 * dimension + 1;
    int scatDimensionNb = (m_betaVector.size() - 1) * 2 + 1;
    CLINT_ASSERT(scatDimension < scatDimensionNb,
                 "Dimension overflow");
    int result = dimension + 1;
    for (int i = 1; i <= scatDimension; i += 2) {
      if (m_tilingDimensions.count(i)) {
        scatDimension += 2;
        result++;
      }
    }
    return result;
  }

  std::vector<int> untiledBetaVector() const;
  std::vector<int> canonicalOriginalBetaVector() const {
    return scop()->canonicalOriginalBetaVector(m_betaVector);
  }

  int minimumValue(int dimIdx) const;
  int maximumValue(int dimIdx) const;

  void resetOccurrence(osl_statement_p stmt, const std::vector<int> &betaVector);
  void resetBetaVector(const std::vector<int> &betaVector);

  enum class Bound {
    LOWER,
    UPPER
  };

  std::vector<int> findBoundlikeForm(Bound bound, int dimIdx, int constValue);


  void debugDumpMinMaxCache(std::ostream &out) {
    out << "Minima: " << std::endl;
    for (auto v : m_cachedDimMins) {
      out << v.first << " : " << v.second << std::endl;
    }
    out << "Maxima: " << std::endl;
    for (auto v : m_cachedDimMaxs) {
      out << v.first << " : " << v.second << std::endl;
    }
    out << std::endl;
  }

signals:
  void pointsChanged();
  void betaChanged();

public slots:

private:
  std::vector<osl_relation_p> m_oslScatterings;
  osl_statement_p m_oslStatement; /// Pointer to the transformed osl statement of this occurrence.  This actually belongs to ClintStmt.
  std::vector<int> m_betaVector;
  std::set<int> m_tilingDimensions;
  ClintStmt *m_statement;
  // FIXME: m_tilingDImensions just duplicates the set of keys of m_tileSizes.
  std::unordered_map<int, unsigned> m_tileSizes;

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
