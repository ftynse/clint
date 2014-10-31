#include "clintdependence.h"
#include "clintstmtoccurrence.h"

ClintDependence::ClintDependence(osl_dependence_p dependence,
                                 ClintStmtOccurrence *source,
                                 ClintStmtOccurrence *target,
                                 QObject *parent) :
  QObject(parent), m_dependence(dependence), m_source(source), m_target(target) {
  CLINT_ASSERT(source, "Dependence source must not be empty");
  CLINT_ASSERT(target, "Dependence source must not be empty");
  CLINT_ASSERT(source->scop() == target->scop(), "Cross-scop dependences are not allowed");
}

std::vector<std::vector<int>>
ClintDependence::projectOn(int horizontalDimIdx, int verticalDimIdx) {
  // XXX: forward-incompatibility
  CLINT_ASSERT(m_dependence->domain->next == nullptr,
               "Union detected in dependence relation; probably Candl was updated. "
               "Changed required in Clint to ensure compatibility.");

  int nbSourceColumns = m_dependence->source_nb_output_dims_domain +
                        m_dependence->source_nb_output_dims_access;

  int horizontalDimS1 = horizontalDimIdx;
  int verticalDimS1   = verticalDimIdx;
  int horizontalDimS2 = horizontalDimS1 + nbSourceColumns;
  int verticalDimS2   = verticalDimS1 + nbSourceColumns;

  std::vector<int> visibleDimensions;
  bool projectHorizontalS1 = m_dependence->source_nb_output_dims_domain > horizontalDimIdx;
  bool projectHorizontalS2 = m_dependence->target_nb_output_dims_domain > horizontalDimIdx;
  bool projectVerticalS1 = m_dependence->source_nb_output_dims_domain > verticalDimIdx;
  bool projectVerticalS2 = m_dependence->target_nb_output_dims_domain > verticalDimIdx;

  if (projectHorizontalS1)
    visibleDimensions.push_back(horizontalDimS1);
  if (projectVerticalS1)
    visibleDimensions.push_back(verticalDimS1);
  if (projectHorizontalS2)
    visibleDimensions.push_back(horizontalDimS2);
  if (projectVerticalS2)
    visibleDimensions.push_back(verticalDimS2);

  osl_relation_p domain = osl_relation_nclone(m_dependence->domain, 1);
  domain->nb_output_dims += domain->nb_input_dims;
  domain->nb_input_dims = 0;
  osl_relation_p ready = oslRelationWithContext(domain, m_source->scop()->fixedContext());
  osl_relation_free(domain);
  std::vector<std::vector<int>> points =
      m_source->program()->enumerator()->enumerate(ready, visibleDimensions);

  return std::move(points);
}

