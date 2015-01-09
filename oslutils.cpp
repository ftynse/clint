#include "oslutils.h"
#include "macros.h"

#include <osl/osl.h>
#include <osl/extensions/extbody.h>

#include <clay/beta.h>

#include <candl/candl.h>
#include <candl/options.h>
#include <candl/dependence.h>
#include <candl/scop.h>

#include <algorithm>
#include <vector>
#include <functional>
#include <utility>

osl_relation_p oslApplyScattering(osl_statement_p stmt) {
  return oslApplyScattering(oslListToVector(stmt->domain),
                            oslListToVector(stmt->scattering));
}

osl_relation_p oslApplyScattering(osl_statement_p stmt, const std::vector<int> &beta) {
  std::vector<osl_relation_p> domains = oslListToVector(stmt->domain);
  std::vector<osl_relation_p> scatterings;
  oslListForeach(stmt->scattering, [&beta,&scatterings](osl_relation_p scattering){
    // If the filter beta is provided, only work with statements that match the given beta.
    if (!beta.empty()) {
      std::vector<int> scatteringBeta = betaExtract(scattering);
      // If the filter beta is longer then the given beta, it does not match.
      if (beta.size() > scatteringBeta.size())
        return;
      if (!std::equal(std::begin(beta), std::end(beta), std::begin(scatteringBeta)))
        return;
      scatterings.push_back(scattering);
    }
  });
  return oslApplyScattering(domains, scatterings);
}

osl_relation_p oslApplyScattering(const std::vector<osl_relation_p> &domains,
                                  const std::vector<osl_relation_p> &scatterings) {
  std::vector<osl_relation_p> domain, scattering;
  std::transform(std::begin(domains), std::end(domains), std::back_inserter(domain),
                 std::bind(&osl_relation_nclone, std::placeholders::_1, 1));
  std::transform(std::begin(scatterings), std::end(scatterings), std::back_inserter(scattering),
                 std::bind(&osl_relation_nclone, std::placeholders::_1, 1));

  osl_relation_p applied_domain = nullptr;
  osl_relation_p result = nullptr;
  // Cartesian product of unions of relations
  for (osl_relation_p domain_union_part : domain) {
    for (osl_relation_p scattering_union_part : scattering) {
      CLINT_ASSERT(domain_union_part->nb_output_dims == scattering_union_part->nb_input_dims,
                   "Scattering is not applicable to the domain: dimensionality mismatch");
      CLINT_ASSERT(domain_union_part->nb_parameters == scattering_union_part->nb_parameters,
                   "Number of parameters doesn't match between domain and scattering");
      CLINT_ASSERT(domain_union_part->nb_input_dims == 0,
                   "Domain should not have input dimensions");

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
      result_union_part->nb_local_dims = domain_union_part->nb_local_dims + scattering_union_part->nb_local_dims;
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

  for (osl_relation_p domain_union_part : domain) {
    osl_relation_free(domain_union_part);
  }
  for (osl_relation_p scattering_union_part : scattering) {
    osl_relation_free(scattering_union_part);
  }

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
  osl_relation_p fixers = osl_relation_pmalloc(relation->precision, num, relation->nb_columns);
  fixers->nb_input_dims = relation->nb_input_dims;
  fixers->nb_output_dims = relation->nb_output_dims;
  fixers->nb_local_dims = relation->nb_local_dims;
  fixers->nb_parameters = relation->nb_parameters;
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

osl_scop_p oslFromCCode(FILE *file) {
  clan_options_p clan_opts = clan_options_malloc();
  clan_opts->castle = 0;
  clan_opts->autoscop = 1;
  clan_opts->extbody = 1;
  osl_scop_p clan_scop = clan_scop_extract(file, clan_opts);
  clan_options_free(clan_opts);
  return clan_scop;
}

osl_scop_p oslFromCCode(char *code) {
  FILE *file = tmpfile();
  fprintf(file, "%s", code);
  fflush(file);
  rewind(file);
  osl_scop_p clan_scop = oslFromCCode(file);
  fclose(file);
  return clan_scop;
}

char *fileContents(FILE *f) {
  if (!f) {
    return nullptr;
  }
  rewind(f);
  fseek(f, 0, SEEK_END);
  size_t fileSize = ftell(f);
  rewind(f);
  char *cstr = (char *) malloc(fileSize * sizeof(char));
  fread(cstr, sizeof(char), fileSize, f);
  return cstr;
}

char *oslToCCode(osl_scop_p scop) {
  CloogState *state = cloog_state_malloc();
  CloogOptions *options = cloog_options_malloc(state);
  options->openscop = 1;
  options->quiet = 1;
//  options->scop = scop;
//  CloogProgram *program = cloog_program_malloc();

  FILE *tmpOslFile = tmpfile();
  osl_scop_print(tmpOslFile, scop);
  fflush(tmpOslFile);
  rewind(tmpOslFile);
  CloogProgram *program = cloog_program_read(tmpOslFile, options);
  fclose(tmpOslFile);

  program = cloog_program_generate(program, options);
  FILE *tmpCloogFile = tmpfile();
  cloog_program_pprint(tmpCloogFile, program, options);
  fflush(tmpCloogFile);

  char *str = fileContents(tmpCloogFile);

  fclose(tmpCloogFile);
  cloog_program_free(program);
  cloog_options_free(options);
  cloog_state_free(state);

  return str;
}

#include <QString>
#include <QChar>
char *escapeHtml(char *str) {
  QString s = QString(str);
  s.replace("<", "&lt;");
  s.replace(">", "&gt;");
  int position = 0;
  while (true) {
    position = s.indexOf(QChar('\n'), position);
    if (position == -1)
      break;
    position++;
    if (position >= s.length())
      break;
    int spaces = 0;
    for (; s[position + spaces].isSpace(); spaces++)
      ;
    for (int i = 0; i < spaces; i++) {
      s.insert(position, "&nbsp;");
    }
    s.remove(position + spaces * 6, spaces);
    position += spaces * 6;
  }
  return strdup(s.toStdString().c_str());
}

// TODO: this function assumes that one beta may have multiple positions in the code,
// but one statement cannot have multiple betas (which is false in case of iss)
// We cannot solve this without support from CLooG since we cannot generate different
// statement contents for different betas of the same statement.
//
// Actually, we can!  All betas in the inputScop may be reified to a statement.
// On the other hand, we do not know how this may affect the output code.  Having CLooG
// generate information about statement position in the code is a better solution.
std::multimap<std::vector<int>, std::pair<int, int>> stmtPositionsInHelper(osl_scop_p inputScop, bool isHtml) {
  std::multimap<std::vector<int>, std::pair<int, int>> positions;

  osl_scop_p scop = osl_scop_clone(inputScop);

  int counter = 0;
  char buffer[40];
  oslListForeach(scop->statement, [&counter,&buffer](osl_statement_p stmt) {
    snprintf(buffer, sizeof(buffer), "__clintstmt__%05d", counter++);
    osl_extbody_p extbody = (osl_extbody_p) osl_generic_lookup(stmt->extension, OSL_URI_EXTBODY);
    if (extbody) {
      osl_body_p body = extbody->body;
      osl_strings_free(body->expression);
      body->expression = osl_strings_encapsulate(strdup(buffer));
    }
    osl_body_p body = (osl_body_p) osl_generic_lookup(stmt->extension, OSL_URI_BODY);
    if (body) {
      osl_strings_free(body->expression);
      body->expression = osl_strings_encapsulate(strdup(buffer));
    }
  });


  char *generatedCode, *originalCode;
  if (isHtml) {
    char *generatedRawCode = oslToCCode(scop);
    char *originalRawCode = oslToCCode(inputScop);
    generatedCode = escapeHtml(generatedRawCode);
    originalCode = escapeHtml(originalRawCode);
    free(generatedRawCode);
    free(originalRawCode);
  } else {
    generatedCode = oslToCCode(scop);
    originalCode = oslToCCode(inputScop);
  }

  char *current = generatedCode;
  char intBuffer[6];
  intBuffer[5] = '\0';
  int lengthDifference = 0;
  while (true) {
    char *found = strstr(current, "__clintstmt__");
    if (!found)
      break;
    strncpy(intBuffer, found + strlen("__clintstmt__"), 5);
    int stmtIdx = atoi(intBuffer);
    osl_statement_p stmt = oslListAt(scop->statement, stmtIdx);
    if (!stmt)
      break;
    std::vector<int> beta = betaExtract(stmt->scattering); // FIXME: multiple scatterings possible
    long generatedStmtStartPos = (found - generatedCode);
    long originalStmtStartPos = generatedStmtStartPos + lengthDifference;
    char *end = strstr(originalCode + originalStmtStartPos, ";");

    if (end == nullptr)
      break;
    long generatedStmtEndPos = generatedStmtStartPos + strlen("__clintstmt__00000");
    long originalStmtEndPos = (end - originalCode) + 1; // Take ';' into account as well
    lengthDifference += (originalStmtEndPos - originalStmtStartPos) - (generatedStmtEndPos - generatedStmtStartPos);
    positions.emplace(beta, std::make_pair(originalStmtStartPos, originalStmtEndPos));
    current = found + (generatedStmtEndPos - generatedStmtEndPos) + 1;
  }

  free(generatedCode);
  free(originalCode);

  return std::move(positions);
}

std::multimap<std::vector<int>, std::pair<int, int>> stmtPositionsInHtml(osl_scop_p inputScop) {
  return stmtPositionsInHelper(inputScop, true);
}

std::multimap<std::vector<int>, std::pair<int, int>> stmtPositionsInCode(osl_scop_p inputScop) {
  return stmtPositionsInHelper(inputScop, false);
}

template <typename T>
void oslListOnlyElement(T **container, int index) {
  if (index < 0 || container == nullptr || *container == nullptr)
    return;
  if (index == 0) {
    osl_relation_free((*container)->next);
    (*container)->next = nullptr;
    return;
  }
  osl_relation_p elementsToDelete = *container;
  osl_relation_p previousElement = oslListAt(elementsToDelete, index - 1); // Checked for 0 before.
  CLINT_ASSERT(previousElement->next != nullptr, "Something weird happend with pointers");
  *container = previousElement->next;
  previousElement->next = previousElement->next->next;
  osl_relation_free(elementsToDelete);
  (*container)->next = nullptr;
}

osl_statement_p oslReifyStatement(osl_statement_p stmt, osl_relation_p scattering_part) {
  osl_statement_p statement = osl_statement_nclone(stmt, 1);
  if (stmt->scattering == nullptr || stmt->scattering->next == nullptr)
    return statement;

  int scatteringIdx = oslListIndexOf(stmt->scattering, scattering_part);
  CLINT_ASSERT(scatteringIdx != -1,
               "Scattering part to reify must be present in the original statement");
  oslListOnlyElement(&statement->scattering, scatteringIdx);
  return statement;
}

osl_statement_p oslReifyStatementDomain(osl_statement_p stmt, osl_relation_p domain_part) {
  osl_statement_p statement = osl_statement_nclone(stmt, 1);
  if (stmt->domain == nullptr || stmt->domain->next == nullptr)
    return statement;

  int domainIdx = oslListIndexOf(stmt->domain, domain_part);
  CLINT_ASSERT(domainIdx != -1,
               "Domain part to reify must be present in the original statement");
  oslListOnlyElement(&statement->domain, domainIdx);
  return statement;
}

osl_dependence_p oslScopDependences(osl_scop_p scop) {
  // TODO: this should be managed by a class that initializes Candl only once

  candl_options_p candl_options = candl_options_malloc();
  candl_options->fullcheck = 1;
  candl_scop_usr_init(scop);
  osl_dependence_p dependences = candl_dependence(scop, candl_options);
  if (dependences)
    candl_dependence_init_fields(scop, dependences);
  candl_scop_usr_cleanup(scop);
  candl_options_free(candl_options);
  return dependences;
}

BetaMap oslBetaMap(osl_scop_p scop) {
  BetaMap betaMap;
  oslListNoSeqCall(scop, [&betaMap](osl_scop_p single_scop) {
    oslListForeach(single_scop->statement, [single_scop,&betaMap](osl_statement_p stmt) {
      oslListForeach(stmt->scattering, [single_scop,stmt,&betaMap](osl_relation_p relation) {
        clay_array_p beta = clay_beta_extract(relation);
        betaMap[betaFromClay(beta)] = std::make_tuple(single_scop, stmt, relation);
        clay_array_free(beta);
      });
    });
  });
  return std::move(betaMap);
}

DependenceMap oslDependenceMap(osl_scop_p scop) {
  // TODO: Candl 0.6.2 does not support computing deps for multiple scops, nor does it support unions.
  // This is a quite inefficient workaround for it, multiple clones of scop with subsequent removals:
  // transform a scop with unions, to a scop without unions by introducing more statements.
  // This function can be moved to Candl to support relation unions.
  // It also requires to change the osl extensions so that it contains pointer to a scattering
  // in addition to the statement.
  // Since dependence domain does not have beta-vectors, it is safe to convert unions to separate
  // statements -- this will not result in beta-vector collisions.
  DependenceMap dependenceMap;
  oslListNoSeqCall(scop, [&dependenceMap](osl_scop_p single_scop) {
    osl_scop_p noUnionScop = osl_scop_clone(single_scop);
    osl_statement_p originalStatements = noUnionScop->statement;
    osl_statement_p newStatements = nullptr;
    osl_statement_p ptr = nullptr;
    oslListForeachSingle(originalStatements, [&newStatements,&ptr](osl_statement_p stmt) {
      oslListForeachSingle(stmt->scattering, [stmt,&newStatements,&ptr](osl_relation_p scattering) {
        osl_statement_p stmtNoScatUnion = oslReifyStatement(stmt, scattering);
        oslListForeachSingle(stmtNoScatUnion->domain,
                             [&stmtNoScatUnion,&newStatements,&ptr](osl_relation_p domain) {
          osl_statement_p stmtNoUnion = oslReifyStatementDomain(stmtNoScatUnion, domain);
          if (newStatements == nullptr) {
            newStatements = stmtNoUnion;
            ptr = stmtNoUnion;
          } else {
            ptr->next = stmtNoUnion;
            ptr = ptr->next;
          }
        });
      });
    });
    noUnionScop->statement = newStatements;
    osl_statement_free(originalStatements);

    osl_dependence_p dependence = oslScopDependences(noUnionScop);

    oslListForeachSingle(dependence, [&dependenceMap](osl_dependence_p dep) {
      CLINT_ASSERT(dep->stmt_source_ptr != nullptr,
                   "Source statement pointer in the osl dependence must be defined");
      CLINT_ASSERT(dep->stmt_target_ptr != nullptr,
                   "Target statement pointer in the osl dependence must be defined");

      std::vector<int> sourceBeta = betaExtract(dep->stmt_source_ptr->scattering);
      std::vector<int> targetBeta = betaExtract(dep->stmt_target_ptr->scattering);
      dep->stmt_source_ptr = nullptr;   // Set up nulls intentionally, the data will be deleted.
      dep->stmt_target_ptr = nullptr;
      dependenceMap.insert(std::make_pair(std::make_pair(sourceBeta, targetBeta), dep));
    });
    osl_scop_free(noUnionScop);
  });
  return std::move(dependenceMap);
}
