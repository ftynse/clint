#ifndef VIZCOORDINATESYSTEM_H
#define VIZCOORDINATESYSTEM_H

#include <QFont>
#include <QGraphicsObject>
#include <QObject>
#include <QSet>

#include "vizprogram.h"
#include "vizscop.h"
#include "vizstatement.h"
#include "vizpolyhedron.h"

// FIXME: hardcoded value of distance between points.
#define DISPLACEMENT 4
#define CLINT_UNDEFINED -1ULL
#define VIZ_POINT_RADIUS   4
#define VIZ_POINT_DISTANCE 16

class VizPolyhedron;

class VizCoordinateSystem : public QGraphicsObject {
  Q_OBJECT
public:
  const static size_t NO_DIMENSION = static_cast<size_t>(-2);

  explicit VizCoordinateSystem(size_t horizontalDimensionIdx = NO_DIMENSION,
                               size_t verticalDimensionIdx = NO_DIMENSION,
                               QGraphicsItem *parent = nullptr);

  QSet<VizScop *> visibleScops() {
    QSet<VizStatement *> stmts = m_program->statementsInCoordinateSystem(this);
    QSet<VizScop *> scops;
    for (VizStatement *stmt : stmts) {
      scops.insert(stmt->scop());
    }
    return scops;
  }

  const VizProgram *program() const {
    return m_program;
  }

  void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget);
  QRectF boundingRect() const;

  bool isHorizontalAxisVisible() const {
    return m_horizontalAxisVisible;
  }

  void setHorizontalAxisVisible(bool visible = true) {
    m_horizontalAxisVisible = visible;
  }

  bool isVerticalAxisVisible() const {
    return m_verticalAxisVisible;
  }

  void setVerticalAxisVisible(bool visible = true) {
    m_verticalAxisVisible = visible;
  }

  size_t horizontalDimensionIdx() const {
    return m_horizontalDimensionIdx;
  }

  size_t verticalDimensionIdx() const {
    return m_verticalDimensionIdx;
  }

  bool projectStatementOccurrence(VizStmtOccurrence *occurrence);

  void extendHorizontally(int minimum, int maximum) {
    m_horizontalMin = std::min(m_horizontalMin, minimum);
    m_horizontalMax = std::max(m_horizontalMax, maximum);
  }

  void extendVertically(int minimum, int maximum) {
    m_verticalMin = std::min(m_verticalMin, minimum);
    m_verticalMax = std::max(m_verticalMax, maximum);
  }

  void setMinMax(int horizontalMinimum, int horizontalMaximum,
                 int verticalMinimum, int verticalMaximum);

  void setMinMax(const std::pair<int, int> &horizontal,
                 const std::pair<int, int> &vertical) {
    setMinMax(horizontal.first, horizontal.second,
              vertical.first, vertical.second);
  }

  int horizontalAxisLength() const {
    return (m_horizontalMax - m_horizontalMin + 3) * VIZ_POINT_DISTANCE;
  }

  int verticalAxisLength() const {
    return (m_verticalMax - m_verticalMin + 3) * VIZ_POINT_DISTANCE;
  }

  int ticMargin() const {
    return 2 * VIZ_POINT_RADIUS;
  }

signals:

public slots:

private:
  std::vector<VizPolyhedron *> m_polyhedra;
  VizProgram *m_program;

  size_t m_horizontalDimensionIdx = CLINT_UNDEFINED;
  size_t m_verticalDimensionIdx   = CLINT_UNDEFINED;

  bool m_horizontalAxisVisible = true;
  bool m_verticalAxisVisible   = true;

  // Default coordinate system is set up at zero.
  // TODO: make this configurable, not all loops may start at origin
  int m_horizontalMin = 0;
  int m_horizontalMax = 0;
  int m_verticalMin   = 0;
  int m_verticalMax   = 0;

  QFont m_font;
};

#endif // VIZCOORDINATESYSTEM_H
