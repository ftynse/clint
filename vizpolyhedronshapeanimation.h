#ifndef VIZPOLYHEDRONSHAPEANIMATION_H
#define VIZPOLYHEDRONSHAPEANIMATION_H

#include <QObject>
#include <QAbstractAnimation>

class VizPolyhedron;

class VizPolyhedronShapeAnimation : public QAbstractAnimation {
public:
  VizPolyhedronShapeAnimation(VizPolyhedron *polyhedron, int duration = 1000);
  virtual ~VizPolyhedronShapeAnimation() = default;
  virtual int duration() const override;
  void setDuration(int duration);

protected:
  virtual void updateCurrentTime(int currentTime) override;

private:
  VizPolyhedron *m_polyhedron;
  int m_duration;
};

#endif // VIZPOLYHEDRONSHAPEANIMATION_H
