#include "macros.h"
#include "transformer.h"

void ClayTransformer::apply(osl_scop_p scop, const Transformation &transformation) {
  int err = 0;
  clay_beta_normalize(scop);
  switch (transformation.kind()) {
  case Transformation::Kind::Fuse:
    err = clay_fuse(scop, ClayBeta(transformation.target()), m_options);
    break;
  case Transformation::Kind::Split:
    err = clay_split(scop, ClayBeta(transformation.target()), transformation.constantAmount(), m_options);
    break;
  case Transformation::Kind::Shift:
  {
    // FIXME: only constant shifting is supported
    clay_list_p list = clay_list_malloc();
    clay_array_p array = clay_array_malloc();
    clay_array_add(array, transformation.constantAmount());
    clay_list_add(list, array);
    err = clay_shift(scop, ClayBeta(transformation.target()), transformation.depth(), list, m_options);
    clay_list_free(list); // List cleans inner arrays, too.
  }
    break;
  case Transformation::Kind::Skew:
  {
    // TODO: skew transformation
//    clay_list_p list = clay_list_malloc();
//    clay_array_p constArray = clay_array_malloc();
//    clay_array_p paramArray = clay_array_malloc();
//    clay_array_p varArray   = clay_array_malloc();
//    clay_list_add(list, constArray);
//    clay_list_add(list, paramArray);
//    clay_list_add(list, varArray);
//    int depth = std::max(transformation.depth(), transformation.secondDepth());
//    err = clay_shift(scop, ClayBeta(transformation.target()), depth, list, m_options);
//    clay_list_free(list);
  }
    break;
  default:
    break;
  }
  CLINT_ASSERT(err == 0, "Error during Clay transformation");
  clay_beta_normalize(scop);
}

std::vector<int> ClayTransformer::transformedBeta(const std::vector<int> &beta, const Transformation &transformation) {
  std::vector<int> tBeta(beta);
  switch (transformation.kind()) {
  case Transformation::Kind::Fuse:
    if (beta > transformation.target()) {
      tBeta[transformation.depth()] -= 1;
    }
    break;
  default:
    break;
  }
  return std::move(tBeta);
}

std::vector<int> ClayTransformer::originalBeta(const std::vector<int> &beta, const Transformation &transformation) {
  std::vector<int> tBeta(beta);
  switch (transformation.kind()) {
  case Transformation::Kind::Fuse:
    if (beta > transformation.target()) {
      tBeta[transformation.depth()] += 1;
    }
    break;
  default:
    break;
  }
  return std::move(tBeta);
}

