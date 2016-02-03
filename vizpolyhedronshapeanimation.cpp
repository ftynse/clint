#include "vizpolyhedronshapeanimation.h"
#include "vizpolyhedron.h"
#include "vizprojection.h"

#ifndef NDEBUG
#include <QtDebug>
#endif

VizPolyhedronShapeAnimation::VizPolyhedronShapeAnimation(VizPolyhedron *polyhedron, int duration) :
  QAbstractAnimation(polyhedron), m_polyhedron(polyhedron), m_duration(duration) {
}

int VizPolyhedronShapeAnimation::duration() const {
  return m_duration;
}

void VizPolyhedronShapeAnimation::setDuration(int duration) {
  m_duration = duration;
}

void VizPolyhedronShapeAnimation::updateCurrentTime(int msecs) {
  Q_UNUSED(msecs);
  m_polyhedron->recomputeShape();
  m_polyhedron->update(m_polyhedron->m_polyhedronShape.boundingRect());
}
