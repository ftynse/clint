#include "vizcoordinatesystem.h"
#include "vizpolyhedron.h"
#include "oslutils.h"
#include "enumerator.h"

#include <clay/beta.h>

#include <QtGui>
#include <QRectF>

VizCoordinateSystem::VizCoordinateSystem(size_t horizontalDimensionIdx, size_t verticalDimensionIdx, QGraphicsItem *parent) :
  QGraphicsObject(parent), m_horizontalDimensionIdx(horizontalDimensionIdx), m_verticalDimensionIdx(verticalDimensionIdx) {

  if (m_horizontalDimensionIdx == NO_DIMENSION)
    m_horizontalAxisVisible = false;
  if (m_verticalDimensionIdx == NO_DIMENSION)
    m_verticalAxisVisible = false;
}

// TODO: should accept VizStatement instead
// VizStatement can enumerate points only once and cache results for multiple projections to use
void VizCoordinateSystem::addStatement__(osl_scop_p scop, osl_statement_p stmt, const std::vector<int> &betaVector) {
  clay_array_p thisBeta = clayBetaFromVector(betaVector);
  // FIXME: not sure if clone here is a good idea
  // enumerator also clones relations, so we have a memory overhead here
  // but it is used to call enumeration only once for all scatterings with the same beta vector
  // Possible solution = add a beta-filter to the enumerator
  osl_statement_p statement = osl_statement_clone(stmt);
  osl_relation_p scattering = statement->scattering;
  statement->scattering = nullptr;
  osl_relation_p scatteringPtr = nullptr;
  osl_relation_p toRemove = nullptr;
  osl_relation_p toRemovePtr = nullptr;

  while (scattering != nullptr) {
    clay_array_p scatteringBeta = clay_beta_extract(scattering);
    bool betaEquals = clay_beta_equals(scatteringBeta, thisBeta);
    if (betaEquals) {
      if (!statement->scattering) {
        statement->scattering = scattering;
        scatteringPtr = scattering;
      } else {
        scatteringPtr->next = scattering;
        scatteringPtr = scatteringPtr->next;
      }
    } else {
      if (!toRemovePtr) {
        toRemove = scattering;
        toRemovePtr = scattering;
      } else {
        toRemovePtr->next = scattering;
        toRemovePtr = toRemovePtr->next;
      }
    }
    // First, select the next scattering.  We can alter the previous afterwards.
    scattering = scattering->next;

    // Make each pointer a proper null-terminated linked list.
    if (betaEquals) {
      if (scatteringPtr)
        scatteringPtr->next = nullptr;
      if (toRemovePtr)
        toRemovePtr->next = nullptr;
    }
  }

  if (toRemove) {
    osl_relation_free(toRemove);
  }

  // Checking if all the relations for the same beta have the same structure.
  // AZ: Not sure if it is theoretically possible: statements with the same beta-vector
  // should normally be in the same part of the domain union and therefore have same
  // number of scattering variables.  If the following assertion ever fails, check the
  // theoretical statement and if it does not hold, perform enumeration separately for
  // each union part.
  auto dimensionalityCheckerFunction = [](osl_relation_p rel, int output_dims,
                                          int input_dims, int parameters) {
    CLINT_ASSERT(rel->nb_output_dims == output_dims,
                 "Dimensionality mismatch, proper polyhedron construction impossible");
    CLINT_ASSERT(rel->nb_input_dims == input_dims,
                 "Dimensionality mismatch, proper polyhedron construction impossible");
    CLINT_ASSERT(rel->nb_parameters == parameters,
                 "Dimensionality mismatch, proper polyhedron construction impossible");
  };
  oslListForeach(statement->domain, dimensionalityCheckerFunction, statement->domain->nb_output_dims,
                 statement->domain->nb_input_dims, statement->domain->nb_parameters);
  oslListForeach(statement->scattering, dimensionalityCheckerFunction, statement->scattering->nb_output_dims,
                 statement->scattering->nb_input_dims, statement->scattering->nb_parameters);


  // TODO: move out osl-related logic to oslutils.h
  osl_relation_p applied = oslApplyScattering(statement);
  // FIXME: hardcoded parameter value
  osl_relation_p fixedParamContext = oslRelationFixAllParameters(scop->context, 4);
  osl_relation_p ready = oslRelationsWithContext(applied, fixedParamContext);

  Enumerator *enumerator = new ISLEnumerator;
  // TODO: move out to oslutils.h
  // transform iterator (alpha only) indices to enumerator (beta-alpha-beta) indices
  // betaDims are found properly iff the dimensionalityChecker assertion holds.
  int horizontalDimOsl    = 1 + 2 * m_horizontalDimensionIdx;
  int verticalDimOsl      = 1 + 2 * m_verticalDimensionIdx;
  int horizontalBetaDim   = 1 + 2 * statement->domain->nb_output_dims + m_horizontalDimensionIdx;
  int verticalBetaDim     = 1 + 2 * statement->domain->nb_output_dims + m_verticalDimensionIdx;

  // Get original and scattered iterator values depending on the axis displayed.
  std::vector<int> visibleDimensions;
  if (!m_horizontalAxisVisible && !m_verticalAxisVisible) {
    // This is just a point, no actual enumeration needed.
    // All the dimensions being projected out, the result of enumeration is a single zero-dimensional point.
  } else if (m_horizontalAxisVisible && !m_verticalAxisVisible) {
    visibleDimensions.push_back(horizontalDimOsl);
    visibleDimensions.push_back(horizontalBetaDim);
  } else if (!m_horizontalAxisVisible && m_verticalAxisVisible) {
    visibleDimensions.push_back(verticalDimOsl);
    visibleDimensions.push_back(verticalBetaDim);
  } else {
    visibleDimensions.push_back(horizontalDimOsl);
    visibleDimensions.push_back(verticalDimOsl);
    visibleDimensions.push_back(horizontalBetaDim);
    visibleDimensions.push_back(verticalBetaDim);
  }

  std::vector<std::vector<int>> points = enumerator->enumerate(ready, std::move(visibleDimensions));

  delete enumerator;
  osl_statement_free(statement);
  // TODO: compute statement min and max, create axes for the coordinate system, etc.

  VizPolyhedron *vp = new VizPolyhedron(this);
  vp->setupFromPoints__(std::move(points));
  QRectF rect = vp->boundingRect();
  // FIXME: hardcoded margin values
  vp->setPos(20 + 3 * m_polyhedra.size(),
             -rect.height() - 3 * m_polyhedra.size());
  m_polyhedra.push_back(vp);
}

void VizCoordinateSystem::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) {
  Q_UNUSED(option);
  Q_UNUSED(widget);

  if (m_horizontalAxisVisible) {
    painter->drawLine(0, 0, 100, 0);
  }

  if (m_verticalAxisVisible) {
    painter->drawLine(0, 0, 0, -100);
  }
}

QRectF VizCoordinateSystem::boundingRect() const {
  return QRectF(0, -100, 100, 100) | childrenBoundingRect();
}
