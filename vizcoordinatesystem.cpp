#include "vizcoordinatesystem.h"
#include "vizpolyhedron.h"
#include "oslutils.h"
#include "enumerator.h"

#include <QtGui>
#include <QRectF>

VizCoordinateSystem::VizCoordinateSystem(size_t horizontalDimensionIdx, size_t verticalDimensionIdx, QGraphicsItem *parent) :
  QGraphicsObject(parent), m_horizontalDimensionIdx(horizontalDimensionIdx), m_verticalDimensionIdx(verticalDimensionIdx) {

  if (m_horizontalDimensionIdx == NO_DIMENSION)
    m_horizontalAxisVisible = false;
  if (m_verticalDimensionIdx == NO_DIMENSION)
    m_verticalAxisVisible = false;
}

bool VizCoordinateSystem::projectStatement(VizStatement *statement, const std::vector<int> &betaVector) {
  // Check if the axes are displayed properly.
  CLINT_ASSERT((betaVector.size() - 1) > m_horizontalDimensionIdx == m_horizontalAxisVisible,
               "Projecting statement on the axis-less dimension")
  CLINT_ASSERT((betaVector.size() - 1) > m_verticalDimensionIdx == m_verticalAxisVisible,
               "Projecting statement on the axis-less dimension")

  std::vector<std::vector<int>> points = statement->projectOn(m_horizontalDimensionIdx,
                                                              m_verticalDimensionIdx,
                                                              betaVector);

  // TODO: compute statement min and max, create axes for the coordinate system, etc.

  if (points.size() == 0)
    return false;

  VizPolyhedron *vp = new VizPolyhedron(this);
  vp->setProjectedPoints(std::move(points));
  QRectF rect = vp->boundingRect();
  // FIXME: hardcoded margin values
  vp->setPos(20 + 3 * m_polyhedra.size(),
             -rect.height() - 3 * m_polyhedra.size());
  m_polyhedra.push_back(vp);
  return true;
}

// TODO: perform this check somewhere in projectScop
#if 0
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
#endif

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
