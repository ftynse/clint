#include "enumerator.h"

#include <algorithm>
#include <functional>
#include <vector>

/// Application-wide ISL context
ISLContextRAII ISLEnumerator::m_islContext;

const int Enumerator::NO_COORD;
const int Enumerator::NO_DIMENSION;

namespace {
isl_stat addISLPointToVector(isl_point *point, void *vect) {
  CLINT_ASSERT(point != nullptr, "Point is nullptr");
  CLINT_ASSERT(vect != nullptr, "Vector is nullptr");

  std::vector<std::vector<int> > &vector = *static_cast<std::vector<std::vector<int> > *>(vect);
  isl_space *space = isl_point_get_space(point);
  const unsigned nbDims = isl_space_dim(space, isl_dim_all);
  std::vector<int> pointVector;
  pointVector.reserve(nbDims);
  for (unsigned i = 0; i < nbDims; i++) {
    isl_val *val = isl_point_get_coordinate_val(point, isl_dim_all, i);
    long value = isl_val_get_num_si(val);
    CLINT_ASSERT(isl_val_get_den_si(val) == 1, "Fractional point");
    CLINT_ASSERT(value <= INT_MAX, "Integer overflow");
    int ivalue = static_cast<int>(value);
    pointVector.push_back(ivalue);
    isl_val_free(val);
  }
  isl_space_free(space);
  isl_point_free(point);
  vector.push_back(std::move(pointVector));

  return isl_stat_ok;
}
} // end anonymous namespace

std::vector<std::vector<int> > ISLEnumerator::enumerate(osl_relation_p relation, const std::vector<int> &dimensions) {
  isl_set *set = setFromOSLRelation(relation);

  std::vector<std::vector<int> > points;
  std::vector<int> allDimensions, dimensionsToProjectOut;
  // Fill non-local dimension indices.
  allDimensions.reserve(relation->nb_columns - 2);
  for (int i = 0; i < relation->nb_columns - 2; i++) {
    if (i < relation->nb_input_dims + relation->nb_output_dims ||
        i >= relation->nb_input_dims + relation->nb_output_dims + relation->nb_local_dims) {
      allDimensions.push_back(i);
    }
  }
  dimensionsToProjectOut.reserve(allDimensions.size() - dimensions.size());
  std::set_difference(std::begin(allDimensions),
                      std::end(allDimensions),
                      std::begin(dimensions),
                      std::end(dimensions),
                      std::back_inserter(dimensionsToProjectOut));

  // Sort dimensions to project out in the descending order.
  // Thus, the projection will not change the indices of the next dimensions to project out.
  std::sort(std::begin(dimensionsToProjectOut), std::end(dimensionsToProjectOut), std::greater<int>());
  for (int dim : dimensionsToProjectOut) {
    isl_dim_type dim_type;
    int index;
    std::tuple<isl_dim_type, int> tuple = ISLEnumerator::dimFromOSL(relation, dim);
    std::tie(dim_type, index) = tuple;
    set = isl_set_project_out(set, dim_type, index, 1);
  }
  isl_set_foreach_point(set, &addISLPointToVector, &points);
  isl_set_free(set);
  return std::move(points); // Force RVO.
}

osl_relation_p ISLEnumerator::scheduledDomain(osl_relation_p domain, osl_relation_p schedule) {
  CLINT_ASSERT(domain->nb_input_dims == 0, "Domain is not a set");
  CLINT_ASSERT(domain->nb_parameters == schedule->nb_parameters,
               "Domain and schedule have different sets of parameters");

  isl_set *islDomain = setFromOSLRelation(domain);
  isl_map *islSchedule = mapFromOSLRelation(schedule);
  isl_set *islScheduledDomain =
      isl_set_apply(islDomain, islSchedule);
  osl_relation_p result = setToOSLRelation(islScheduledDomain);
  isl_set_free(islScheduledDomain);
  return result;
}

ISLEnumerator::~ISLEnumerator() {
}
