#include "vizcoordinatesystem.h"
#include "vizpolyhedron.h"
#include "vizstmtoccurrence.h"
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

bool VizCoordinateSystem::projectStatementOccurrence(VizStmtOccurrence *occurrence) {
  // Check if the axes are displayed properly.
  CLINT_ASSERT((occurrence->dimensionality() > m_horizontalDimensionIdx) == m_horizontalAxisVisible,
               "Projecting statement on the axis-less dimension");
  CLINT_ASSERT((occurrence->dimensionality() > m_verticalDimensionIdx) == m_verticalAxisVisible,
               "Projecting statement on the axis-less dimension");

  std::vector<std::vector<int>> points = occurrence->projectOn(m_horizontalDimensionIdx,
                                                               m_verticalDimensionIdx);

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
