#ifndef CLINTBETA_H
#define CLINTBETA_H

#include <vector>

#include <QMetaType>

#include "macros.h"

// TODO: pass Clint* to use of ClintBeta instead of std::vector<int>
class ClintBeta {
public:
  ClintBeta() = default;
  ClintBeta(const std::vector<int> &beta);
  explicit operator std::vector<int> ();

  int &operator[] (int index) {
    CLINT_ASSERT(index >= 0, "Beta element index must be positive");
    CLINT_ASSERT(index < m_beta.size(), "Beta-vector index overflow");
    return m_beta.at(index);
  }

  bool operator== (const ClintBeta &other) {
    return m_beta == other.m_beta;
  }

  bool operator< (const ClintBeta &other) {
    return m_beta < other.m_beta;
  }

private:
  std::vector<int> m_beta;
};

Q_DECLARE_METATYPE(ClintBeta)

#endif // CLINTBETA_H
