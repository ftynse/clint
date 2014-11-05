#include "macros.h"
#include "vizproperties.h"

#include <QtCore>

const size_t VizProperties::NO_DIMENSION;
const size_t VizProperties::UNDEFINED_DIMENSION;

VizProperties::VizProperties(QObject *parent) :
  QObject(parent) {
}

void VizProperties::setPolyhedronOffset(double offset) {
  m_polyhedronOffset = offset;
  emit vizPropertyChanged();
}

void VizProperties::setPointRadius(double radius) {
  m_pointRadius = radius;
  emit vizPropertyChanged();
}

void VizProperties::setPointDistance(double distance) {
  m_pointDistance = distance;
  emit vizPropertyChanged();
}

void VizProperties::setCoordinateSystemMargin(double margin) {
  m_coordinateSystemMargin = margin;
  emit vizPropertyChanged();
}

QDomElement VizProperties::toXML(QDomDocument &doc) const {
  QDomElement element = doc.createElement("vizproperties");
  element.setAttribute("polyhedronOffset", m_polyhedronOffset);
  element.setAttribute("pointRadius", m_pointRadius);
  element.setAttribute("pointDistance", m_pointDistance);
  element.setAttribute("coordinateSystemMargin", m_coordinateSystemMargin);
  return element;
}

void VizProperties::fromXML(const QDomElement &element) {
  CLINT_ASSERT(element.tagName() == "vizproperties", "Wrong XML element given");
  QString polyhedronOffsetString       = element.attribute("polyhedronOffset");
  QString pointRadiusString            = element.attribute("pointRadius");
  QString pointDistanceString          = element.attribute("pointDistance");
  QString coordinateSystemMarginString = element.attribute("coordinateSystemMargin");
  bool ok = true;
  m_polyhedronOffset       = polyhedronOffsetString.toDouble(&ok);
  m_pointRadius            = pointRadiusString.toDouble(&ok);
  m_pointDistance          = pointDistanceString.toDouble(&ok);
  m_coordinateSystemMargin = coordinateSystemMarginString.toDouble(&ok);
  CLINT_ASSERT(ok, "Could not find or interpret one of the viz properties");
}
