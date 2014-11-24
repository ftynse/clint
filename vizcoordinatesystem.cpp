#include "vizcoordinatesystem.h"
#include "vizpolyhedron.h"
#include "vizprojection.h"
#include "clintstmtoccurrence.h"
#include "clintdependence.h"
#include "oslutils.h"
#include "enumerator.h"

#include <QtCore>
#include <QtGui>
#include <QtWidgets>

VizCoordinateSystem::VizCoordinateSystem(VizProjection *projection, size_t horizontalDimensionIdx, size_t verticalDimensionIdx, QGraphicsItem *parent) :
  QGraphicsObject(parent), m_projection(projection), m_horizontalDimensionIdx(horizontalDimensionIdx), m_verticalDimensionIdx(verticalDimensionIdx) {

  if (m_horizontalDimensionIdx == VizProperties::NO_DIMENSION)
    m_horizontalAxisVisible = false;
  if (m_verticalDimensionIdx == VizProperties::NO_DIMENSION)
    m_verticalAxisVisible = false;

  m_font = qApp->font();  // Setting up default font for the view.  Can be adjusted afterwards.
}

bool VizCoordinateSystem::projectStatementOccurrence(ClintStmtOccurrence *occurrence) {
  // Check if the axes are displayed properly.
  CLINT_ASSERT((occurrence->dimensionality() > m_horizontalDimensionIdx) == m_horizontalAxisVisible,
               "Projecting statement on the axis-less dimension");
  CLINT_ASSERT((occurrence->dimensionality() > m_verticalDimensionIdx) == m_verticalAxisVisible,
               "Projecting statement on the axis-less dimension");

  std::vector<std::vector<int>> points = occurrence->projectOn(m_horizontalDimensionIdx,
                                                               m_verticalDimensionIdx);

  if (points.size() == 0)
    return false;

  // Add iterator variable names to the list.
  if (m_horizontalAxisVisible) {
    const char *horizontalName = occurrence->statement()->dimensionName(m_horizontalDimensionIdx).c_str();
    if (m_horizontalName.size() == 0) {
      m_horizontalName = QString(horizontalName);
    } else {
      m_horizontalName += QString(",%1").arg(horizontalName);
    }
  }
  if (m_verticalAxisVisible) {
    const char *verticalName = occurrence->statement()->dimensionName(m_verticalDimensionIdx).c_str();
    if (m_verticalName.size() == 0) {
      m_verticalName = QString(verticalName);
    } else {
      m_verticalName += QString(",%1").arg(verticalName);
    }
  }

  int occurrenceHorizontalMin = occurrence->minimumValue(m_horizontalDimensionIdx);
  int occurrenceHorizontalMax = occurrence->maximumValue(m_horizontalDimensionIdx);
  int occurrenceVerticalMin   = occurrence->minimumValue(m_verticalDimensionIdx);
  int occurrenceVerticalMax   = occurrence->maximumValue(m_verticalDimensionIdx);
  VizPolyhedron *vp = new VizPolyhedron(this);
  vp->setProjectedPoints(std::move(points),
                         occurrenceHorizontalMin,
                         occurrenceHorizontalMax,
                         occurrenceVerticalMin,
                         occurrenceVerticalMax);
  m_polyhedra.push_back(vp);
  setMinMax(std::min(occurrenceHorizontalMin, m_horizontalMin),
            std::max(occurrenceHorizontalMax, m_horizontalMax),
            std::min(occurrenceVerticalMin, m_verticalMin),
            std::max(occurrenceVerticalMax, m_verticalMax));

  // Setting up internal dependences.
  std::unordered_set<ClintDependence *> dependences =
      occurrence->scop()->internalDependences(occurrence);
  for (ClintDependence *dependence : dependences) {
    std::vector<std::vector<int>> lines =
        dependence->projectOn(m_horizontalDimensionIdx, m_verticalDimensionIdx);
    vp->setInternalDependences(lines);
  }

  return true;
}

void VizCoordinateSystem::setMinMax(int horizontalMinimum, int horizontalMaximum,
                                    int verticalMinimum, int verticalMaximum) {
  const double pointDistance = m_projection->vizProperties()->pointDistance();
  m_horizontalMin = horizontalMinimum;
  m_horizontalMax = horizontalMaximum;
  m_verticalMin   = verticalMinimum;
  m_verticalMax   = verticalMaximum;
  for (size_t i = 0, iend = m_polyhedra.size(); i < iend; i++) {
    VizPolyhedron *vph = m_polyhedra.at(i);
    double offset = m_projection->vizProperties()->polyhedronOffset() * i;
    vph->setPos(offset + (vph->localHorizontalMin() - horizontalMinimum + 1) * pointDistance,
                -(offset + (vph->localVerticalMin() - verticalMinimum + 1) * pointDistance));
    vph->setZValue(m_polyhedra.size() + 1 - i);
  }
}

void VizCoordinateSystem::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) {
  Q_UNUSED(option);
  Q_UNUSED(widget);

  const double pointRadius = m_projection->vizProperties()->pointRadius();
  const double pointDistance = m_projection->vizProperties()->pointDistance();

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
    triangle.append(QPointF(length - 2 * pointRadius, pointRadius));
    triangle.append(QPointF(length - 2 * pointRadius, -pointRadius));
    triangle.append(triangle.front());
    painter->drawConvexPolygon(triangle);

    // Draw tics.
    for (int i = m_horizontalMin; i <= m_horizontalMax; i++) {
      int pos = (1 + (i - m_horizontalMin)) * pointDistance;
      painter->drawLine(pos, -pointRadius / 2., pos, pointRadius / 2.);
      QString ticText = QString("%1").arg(i);
      painter->drawText(pos - pointDistance / 2.,
                        ticMargin(),
                        pointDistance,
                        fm.lineSpacing(),
                        Qt::AlignHCenter | Qt::AlignBottom | Qt::TextDontClip, ticText);
    }

    // Draw label.
    int pos = (3 + m_horizontalMax - m_horizontalMin) * pointDistance;
    painter->drawText(pos - pointDistance / 2.,
                      ticMargin(),
                      fm.width(m_horizontalName),
                      fm.lineSpacing(),
                      Qt::AlignHCenter | Qt::AlignBottom | Qt::TextDontClip, m_horizontalName);
  }

  if (m_verticalAxisVisible) {
    // Draw axis.
    int length = verticalAxisLength();
    painter->drawLine(0, 0, 0, -length);
    // Draw arrow.
    QPolygonF triangle;
    triangle.append(QPointF(0, -length));
    triangle.append(QPointF(pointRadius, -length + 2 * pointRadius));
    triangle.append(QPointF(-pointRadius, -length + 2 * pointRadius));
    triangle.append(triangle.front());
    painter->drawConvexPolygon(triangle);

    // Draw tics.
    for (int i = m_verticalMin; i <= m_verticalMax; i++) {
      int pos = -(1 + (i - m_verticalMin)) * pointDistance;
      painter->drawLine(-pointRadius / 2., pos, pointRadius / 2., pos);
      QString ticText = QString("%1").arg(i);
      int textWidth = fm.width(ticText);
      painter->drawText(-textWidth - ticMargin(),
                        pos - pointDistance / 2.,
                        textWidth,
                        pointDistance,
                        Qt::AlignVCenter | Qt::AlignRight | Qt::TextDontClip, ticText);
    }

    // Draw label.
    int pos = -(3 + m_verticalMax - m_verticalMin) * pointDistance;
    int textWidth = fm.width(m_verticalName);
    painter->drawText(-textWidth - ticMargin(),
                      pos - pointDistance / 2.,
                      textWidth,
                      pointDistance,
                      Qt::AlignVCenter | Qt::AlignRight | Qt::TextDontClip, m_verticalName);
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

int VizCoordinateSystem::horizontalAxisLength() const {
  return (m_horizontalMax - m_horizontalMin + 3) * m_projection->vizProperties()->pointDistance();
}

int VizCoordinateSystem::verticalAxisLength() const {
  return (m_verticalMax - m_verticalMin + 3) * m_projection->vizProperties()->pointDistance();
}

int VizCoordinateSystem::ticMargin() const {
  return 2 * m_projection->vizProperties()->pointRadius();
}
