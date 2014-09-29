#include "oslutils.h"
#include "macros.h"

#include <osl/osl.h>

#include <clay/beta.h>

osl_relation_p oslApplyScattering(osl_statement_p stmt) {
  osl_relation_p domain_union_part, scattering_union_part;
  osl_relation_p scattering = osl_relation_clone(stmt->scattering);
  osl_relation_p domain = osl_relation_clone(stmt->domain);

  CLINT_ASSERT(stmt->domain->nb_parameters == stmt->scattering->nb_parameters,
               "Statement relations use different parameters");

  int domainMaxNbLocalDim = 0, scatteringMaxNbLocalDim = 0;
  LL_FOREACH(domain_union_part, domain) {
    if (domainMaxNbLocalDim < domain_union_part->nb_local_dims) {
      domainMaxNbLocalDim = domain_union_part->nb_local_dims;
    }
  }
  LL_FOREACH(scattering_union_part, scattering) {
    if (scatteringMaxNbLocalDim < scattering_union_part->nb_local_dims) {
      scatteringMaxNbLocalDim = scattering_union_part->nb_local_dims;
    }
  }

  LL_FOREACH(domain_union_part, domain) {
    // Make space for local dimensions (move current to the end).
    for (int i = 0; i < scatteringMaxNbLocalDim; i++) {
      osl_relation_insert_blank_column(domain_union_part,
                                       1 + domain_union_part->nb_input_dims);
    }
    domain_union_part->nb_local_dims += scatteringMaxNbLocalDim;
  }

  LL_FOREACH(scattering_union_part, scattering) {
    // Make space for extra local dimensions (leave current at the beginning).
    for (int i = 0; i < domainMaxNbLocalDim; i++) {
      osl_relation_insert_blank_column(scattering_union_part,
                                       scattering_union_part->nb_columns - 1 -
                                       scattering_union_part->nb_parameters);
    }
    scattering_union_part->nb_local_dims += domainMaxNbLocalDim;
  }

  osl_relation_p applied_domain = nullptr;
  osl_relation_p result = nullptr;
  // Cartesian product of unions of relations
  LL_FOREACH(domain_union_part, domain) {
    LL_FOREACH(scattering_union_part, scattering) {
      CLINT_ASSERT(domain_union_part->nb_output_dims == scattering_union_part->nb_input_dims,
                   "Scattering is not applicable to the domain: dimensionality mismatch");
      CLINT_ASSERT(domain_union_part->nb_parameters == scattering_union_part->nb_parameters,
                   "Number of parameters doesn't match between domain and scattering");
      CLINT_ASSERT(domain_union_part->nb_input_dims == 0,
                   "Domain should not have input dimensions")

      // Prepare a domain relation for concatenation with a scattering relation.
      // Add columns to accomodate local dimensions of the scattering relation (l's).
      // Local dimensions are intrinsic to each relation and should not overlap.
      // Prepend local dimensions to the domain and append to the scattering.
      osl_relation_p domain_part = osl_relation_nclone(domain_union_part, 1);
      osl_relation_p scattering_part = osl_relation_nclone(scattering_union_part, 1);
      int domain_local_index = 1 + domain_part->nb_output_dims + domain_part->nb_input_dims;
      for (int i = 0; i < scattering_union_part->nb_local_dims; i++) {
        osl_relation_insert_blank_column(domain_part, domain_local_index);
      }
      // Add columns to accommodate output dimensions of the scattering relation (c's).
      for (int i = 0; i < scattering_union_part->nb_output_dims; i++) {
        osl_relation_insert_blank_column(domain_part, 1);
      }
      domain_part->nb_input_dims  = scattering_union_part->nb_input_dims;
      domain_part->nb_output_dims = scattering_union_part->nb_output_dims;
      domain_part->nb_local_dims  = domain_part->nb_local_dims + scattering_union_part->nb_local_dims;
      domain_part->type           = OSL_TYPE_SCATTERING;

      // Append local dimensions to the scatteirng;
      int scattering_local_index = 1 + scattering_part->nb_input_dims
          + scattering_part->nb_output_dims + scattering_part->nb_local_dims;
      for (int i = 0; i < domain_union_part->nb_local_dims; i++) {
        osl_relation_insert_blank_column(scattering_part, scattering_local_index);
      }
      scattering_part->nb_local_dims += domain_union_part->nb_local_dims;

      // Create a scattered domain relation.
      osl_relation_p result_union_part = osl_relation_concat_constraints(domain_part, scattering_part);
      result_union_part->nb_output_dims = scattering_union_part->nb_input_dims + scattering_union_part->nb_output_dims;
      result_union_part->nb_input_dims = 0;
      result_union_part->nb_local_dims = domain_union_part->nb_local_dims;
      result_union_part->nb_parameters = domain_union_part->nb_parameters;
      result_union_part->type = OSL_TYPE_DOMAIN;
      result_union_part->next = nullptr;
      osl_relation_integrity_check(result_union_part, OSL_TYPE_DOMAIN,
                                   scattering_union_part->nb_input_dims + scattering_union_part->nb_output_dims,
                                   0, scattering_union_part->nb_parameters);
      if (result == nullptr) {
        applied_domain = result_union_part;
        result = applied_domain;
      } else {
        applied_domain->next = result_union_part;
        applied_domain = result_union_part;
      }
      osl_relation_free(domain_part);
      osl_relation_free(scattering_part);
    }
  }

  osl_relation_free(domain);
  osl_relation_free(scattering);

  return result;
}

osl_relation_p oslRelationWithContext(osl_relation_p relation, osl_relation_p context) {
  CLINT_ASSERT(relation->nb_parameters == context->nb_parameters,
               "Context doesn't involve all parameters");
  osl_relation_p result = nullptr, ptr = nullptr;
  osl_relation_p context_part;
  LL_FOREACH(context_part, context) {
    osl_relation_p result_part = osl_relation_nclone(relation, 1);
    const int idx = 1 + result_part->nb_input_dims + result_part->nb_output_dims + result_part->nb_local_dims;
    for (int i = 0; i < context_part->nb_rows; i++) {
      osl_relation_insert_blank_row(result_part, -1);
      for (int j = 0; j < context_part->nb_parameters; j++) {
        osl_int_assign(result_part->precision,
                       &result_part->m[result_part->nb_rows - 1][idx + j],
                       context_part->m[i][1 + j]);
      }
      osl_int_assign(result_part->precision,
                     &result_part->m[result_part->nb_rows - 1][0],
                     context_part->m[i][0]);
      osl_int_assign(result_part->precision,
                     &result_part->m[result_part->nb_rows - 1][result_part->nb_columns - 1],
                     context_part->m[i][context_part->nb_columns - 1]);
    }

    if (result == nullptr) {
      result = result_part;
      ptr = result;
    } else {
      ptr->next = result_part;
      ptr = ptr->next;
    }
  }
  return result;
}

osl_relation_p oslRelationFixParameters(osl_relation_p relation, const std::vector<std::pair<bool, int>> &values) {
  if (relation == nullptr) {
    return nullptr;
  }

  CLINT_ASSERT(values.size() == relation->nb_parameters, "Not all parameters provided with values");
  size_t num = std::count_if(std::begin(values), std::end(values), [](const std::pair<bool, int> &it) { return it.first; });
  osl_relation_p fixers = osl_relation_malloc(num, relation->nb_columns);
  size_t idx = 0;
  size_t firstParam = relation->nb_input_dims + relation->nb_output_dims + relation->nb_local_dims + 1;
  for (size_t i = 0; i < values.size(); i++) {
    const std::pair<bool, int> &v = values.at(i);
    if (!v.first) {
      continue;
    }
    osl_int_set_si(fixers->precision, &fixers->m[idx][0], 0);                             // Equality of
    osl_int_set_si(fixers->precision, &fixers->m[idx][firstParam + i], -1);               // Parameter to
    osl_int_set_si(fixers->precision, &fixers->m[idx][fixers->nb_columns - 1], v.second); // Value
    ++idx;
  }
  osl_relation_p result = osl_relation_concat_constraints(relation, fixers);
  osl_relation_free(fixers);
  result->type           = relation->type;
  result->nb_input_dims  = relation->nb_input_dims;
  result->nb_output_dims = relation->nb_output_dims;
  result->nb_local_dims  = relation->nb_local_dims;
  result->nb_parameters  = relation->nb_parameters;
  return result;
}

osl_relation_p oslRelationFixAllParameters(osl_relation_p relation, int value) {
  std::vector<std::pair<bool, int>> values;
  values.resize(relation->nb_parameters, std::make_pair(true, value));
  return oslRelationFixParameters(relation, values);
}

std::vector<int> betaFromClay(clay_array_p beta) {
  std::vector<int> betavector;
  betavector.reserve(beta->size);
  std::copy(beta->data, beta->data + beta->size, std::back_inserter(betavector));
  return std::move(betavector);
}

clay_array_p clayBetaFromVector(const std::vector<int> &betaVector) {
  clay_array_p beta = clay_array_malloc();
  for (int i : betaVector) {
    clay_array_add(beta, i);
  }
  return beta;
}

BetaMap oslBetaMap(osl_scop_p scop) {
  BetaMap betaMap;
  oslListNoSeqCall(scop, [&betaMap](osl_scop_p single_scop) {
    oslListForeach(single_scop->statement, [single_scop,&betaMap](osl_statement_p stmt) {
      oslListForeach(stmt->scattering, [single_scop,stmt,&betaMap](osl_relation_p relation) {
        clay_array_p beta = clay_beta_extract(relation);
        betaMap[betaFromClay(beta)] = std::make_tuple(single_scop, stmt, relation);
      });
    });
  });
  return std::move(betaMap);
}
