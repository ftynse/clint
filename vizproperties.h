#ifndef VIZPROPERTIES_H
#define VIZPROPERTIES_H

#include <QObject>
#include <QColor>
#include <QtXml>

#include <vector>

class VizProperties : public QObject {
  Q_OBJECT
public:
  explicit VizProperties(QObject *parent = 0);
  /*inline*/ double polyhedronOffset() const {
    return m_polyhedronOffset;
  }
  /*inline*/ double pointRadius() const {
    return m_pointRadius;
  }
  /*inline*/ double pointDistance() const {
    return m_pointDistance;
  }
  /*inline*/ double coordinateSystemMargin() const {
    return m_coordinateSystemMargin;
  }

  QColor color(const std::vector<int> &beta) const {
    int id = 0;
    for (int i = 0; i < beta.size(); i++) {
      id += beta[i] * pow(10, i % 7);
    }
    id = id % 24;
    id = (id * 7) % 24; // distribute far from each other
    return QColor::fromHsv(id * 15, 60, 200, 127);
  }

  bool filledPolygons() const {
    return m_filledPolygons;
  }

  bool filledPoints() const {
    return m_filledPoints;
  }

  const static size_t UNDEFINED_DIMENSION = (size_t) -1;
  const static size_t NO_DIMENSION        = (size_t) -2;

  QDomElement toXML(QDomDocument &doc) const;
  void fromXML(const QDomElement &xml);

  Q_PROPERTY(double polyhedronOffset
             READ polyhedronOffset
             WRITE setPolyhedronOffset
             NOTIFY vizPropertyChanged)
  Q_PROPERTY(double pointRadius
             READ pointRadius
             WRITE setPointRadius
             NOTIFY vizPropertyChanged)
  Q_PROPERTY(double pointDistance
             READ pointDistance
             WRITE setPointDistance
             NOTIFY vizPropertyChanged)
  Q_PROPERTY(double coordinateSystemMargin
             READ coordinateSystemMargin
             WRITE setCoordinateSystemMargin
             NOTIFY vizPropertyChanged)

signals:
  void vizPropertyChanged();

public slots:
  void setPolyhedronOffset(double offset);
  void setPointRadius(double radius);
  void setPointDistance(double distance);
  void setCoordinateSystemMargin(double margin);

private:
  double m_polyhedronOffset       = 4.0;
  double m_pointRadius            = 4.0;
  double m_pointDistance          = 16.0;
  double m_coordinateSystemMargin = 5.0;

  bool m_filledPolygons           = true;
  bool m_filledPoints             = false;
};

#endif // VIZPROPERTIES_H
