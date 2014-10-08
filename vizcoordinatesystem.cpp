#include "vizcoordinatesystem.h"
#include "vizpolyhedron.h"
#include "vizstmtoccurrence.h"
#include "oslutils.h"
#include "enumerator.h"

#include <QtCore>
#include <QtGui>
#include <QtWidgets>

VizCoordinateSystem::VizCoordinateSystem(size_t horizontalDimensionIdx, size_t verticalDimensionIdx, QGraphicsItem *parent) :
  QGraphicsObject(parent), m_horizontalDimensionIdx(horizontalDimensionIdx), m_verticalDimensionIdx(verticalDimensionIdx) {

  if (m_horizontalDimensionIdx == NO_DIMENSION)
    m_horizontalAxisVisible = false;
  if (m_verticalDimensionIdx == NO_DIMENSION)
    m_verticalAxisVisible = false;

  m_font = qApp->font();  // Setting up default font for the view.  Can be adjusted afterwards.
}

bool VizCoordinateSystem::projectStatementOccurrence(VizStmtOccurrence *occurrence) {
  // Check if the axes are displayed properly.
  CLINT_ASSERT((occurrence->dimensionality() > m_horizontalDimensionIdx) == m_horizontalAxisVisible,
               "Projecting statement on the axis-less dimension");
  CLINT_ASSERT((occurrence->dimensionality() > m_verticalDimensionIdx) == m_verticalAxisVisible,
               "Projecting statement on the axis-less dimension");

  std::vector<std::vector<int>> points = occurrence->projectOn(m_horizontalDimensionIdx,
                                                               m_verticalDimensionIdx);

  if (points.size() == 0)
    return false;

  // TODO: set up iterator names

  VizPolyhedron *vp = new VizPolyhedron(this);
  vp->setProjectedPoints(std::move(points),
                         occurrence->minimumValue(m_horizontalDimensionIdx),
                         occurrence->minimumValue(m_verticalDimensionIdx));
  QRectF rect = vp->boundingRect();
  // FIXME: hardcoded margin values, and -4 taken from polyhedra boundary size
  vp->setPos(3 * m_polyhedra.size(),
             -rect.height() - 4 - 3 * m_polyhedra.size());
  m_polyhedra.push_back(vp);
  return true;
}

void VizCoordinateSystem::setMinMax(int horizontalMinimum, int horizontalMaximum,
                                    int verticalMinimum, int verticalMaximum) {
  m_horizontalMin = horizontalMinimum;
  m_horizontalMax = horizontalMaximum;
  m_verticalMin   = verticalMinimum;
  m_verticalMax   = verticalMaximum;
  for (VizPolyhedron *vph : m_polyhedra) {
    vph->adjustProjectedPoints(horizontalMinimum, verticalMinimum);
  }
}

void VizCoordinateSystem::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) {
  Q_UNUSED(option);
  Q_UNUSED(widget);

  QFontMetrics fm = painter->fontMetrics();
  painter->setBrush(Qt::black);
  painter->setFont(m_font);
  painter->setRenderHint(QPainter::Antialiasing);
  painter->setRenderHint(QPainter::TextAntialiasing);

  if (m_horizontalAxisVisible) {
    // Draw axis.
    int length = horizontalAxisLength();
    painter->drawLine(0, 0, length, 0);
    // Draw arrow.
    QPolygonF triangle;
    triangle.append(QPointF(length, 0));
    triangle.append(QPointF(length - 2 * VIZ_POINT_RADIUS, VIZ_POINT_RADIUS));
    triangle.append(QPointF(length - 2 * VIZ_POINT_RADIUS, -VIZ_POINT_RADIUS));
    triangle.append(triangle.front());
    painter->drawConvexPolygon(triangle);

    // Draw tics.
    for (int i = m_horizontalMin; i <= m_horizontalMax; i++) {
      int pos = (1 + (i - m_horizontalMin)) * VIZ_POINT_DISTANCE;
      painter->drawLine(pos, -VIZ_POINT_RADIUS / 2., pos, VIZ_POINT_RADIUS / 2.);
      QString ticText = QString("%1").arg(i);
      painter->drawText(pos - VIZ_POINT_DISTANCE / 2.,
                        ticMargin(),
                        VIZ_POINT_DISTANCE,
                        fm.lineSpacing(),
                        Qt::AlignHCenter | Qt::AlignBottom | Qt::TextDontClip, ticText);
    }
  }

  if (m_verticalAxisVisible) {
    // Draw axis.
    int length = verticalAxisLength();
    painter->drawLine(0, 0, 0, -length);
    // Draw arrow.
    QPolygonF triangle;
    triangle.append(QPointF(0, -length));
    triangle.append(QPointF(VIZ_POINT_RADIUS, -length + 2 * VIZ_POINT_RADIUS));
    triangle.append(QPointF(-VIZ_POINT_RADIUS, -length + 2 * VIZ_POINT_RADIUS));
    triangle.append(triangle.front());
    painter->drawConvexPolygon(triangle);

    // Draw tics.
    for (int i = m_verticalMin; i <= m_verticalMax; i++) {
      int pos = -(1 + (i - m_verticalMin)) * VIZ_POINT_DISTANCE;
      painter->drawLine(-VIZ_POINT_RADIUS / 2., pos, VIZ_POINT_RADIUS / 2., pos);
      QString ticText = QString("%1").arg(i);
      int textWidth = fm.width(ticText);
      painter->drawText(-textWidth - ticMargin(),
                        pos - VIZ_POINT_DISTANCE / 2.,
                        textWidth,
                        VIZ_POINT_DISTANCE,
                        Qt::AlignVCenter | Qt::AlignRight | Qt::TextDontClip, ticText);
    }
  }
}

QRectF VizCoordinateSystem::boundingRect() const {
  int width  = horizontalAxisLength();
  int height = verticalAxisLength();
  int left   = 0;
  int top    = -height;

  QFontMetrics fm(m_font);
  int ticWidth = 0;
  for (int i = m_verticalMin; i <= m_verticalMax; i++) {
    ticWidth = std::max(ticWidth, fm.width(QString("%1").arg(i)));
  }
  ticWidth += ticMargin();

  width += ticWidth;
  left -= ticWidth;
  height += ticMargin() + fm.height();
  return QRectF(left, top, width, height) | childrenBoundingRect();
}
