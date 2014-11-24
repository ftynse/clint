#include "clintbeta.h"

#include <algorithm>
#include <iterator>

ClintBeta::ClintBeta(const std::vector<int> &beta) {
  m_beta.reserve(beta.size());
  std::copy(std::begin(beta), std::end(beta), std::back_inserter(m_beta));
}

ClintBeta::operator std::vector<int>() {
  return m_beta; // new copy constructed
}
