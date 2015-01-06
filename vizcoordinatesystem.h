#ifndef VIZCOORDINATESYSTEM_H
#define VIZCOORDINATESYSTEM_H

#include <QFont>
#include <QGraphicsObject>
#include <QObject>
#include <QSet>

#include "clintprogram.h"
#include "clintscop.h"
#include "clintstmt.h"
#include "vizproperties.h"

class VizPolyhedron;
class VizProjection;

class VizCoordinateSystem : public QGraphicsObject {
  Q_OBJECT
public:
  explicit VizCoordinateSystem(VizProjection *projection,
                               size_t horizontalDimensionIdx = VizProperties::NO_DIMENSION,
                               size_t verticalDimensionIdx = VizProperties::NO_DIMENSION,
                               QGraphicsItem *parent = nullptr);

  QSet<ClintScop *> visibleScops() {
    QSet<ClintStmt *> stmts = m_program->statementsInCoordinateSystem(this);
    QSet<ClintScop *> scops;
    for (ClintStmt *stmt : stmts) {
      scops.insert(stmt->scop());
    }
    return scops;
  }

  const ClintProgram *program() const {
    return m_program;
  }

  VizProjection *projection() const {
    return m_projection;
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

  QRectF coordinateSystemRect() const;

  bool projectStatementOccurrence(ClintStmtOccurrence *occurrence);

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

  void minMax(int &horizontalMinimum, int &horizontalMaximum,
              int &verticalMinimum, int &verticalMaximum) {
    horizontalMinimum = m_horizontalMin;
    horizontalMaximum = m_horizontalMax;
    verticalMinimum = m_verticalMin;
    verticalMaximum = m_verticalMax;
  }

  bool isEmpty() const {
    return m_polyhedra.empty();
  }

  int countPolyhedra() const {
    return m_polyhedra.size();
  }

  std::vector<int> betaPrefix() const;

  int horizontalAxisLength() const;
  int verticalAxisLength() const;
  int ticMargin() const;

  void polyhedronUpdated(VizPolyhedron *polyhedron);
  void setPolyhedronCoordinates(VizPolyhedron *polyhedron, int horizontal, int vertical,
                                bool ignoreHorizontal = false, bool ignoreVertical = false);
  void reparentPolyhedron(VizPolyhedron *polyhedron);
  void resetPolyhedronPos(VizPolyhedron *polyhedron);
signals:

public slots:

private:
  std::vector<VizPolyhedron *> m_polyhedra;
  ClintProgram *m_program;
  VizProjection *m_projection;

  size_t m_horizontalDimensionIdx = VizProperties::UNDEFINED_DIMENSION;
  size_t m_verticalDimensionIdx   = VizProperties::UNDEFINED_DIMENSION;

  bool m_horizontalAxisVisible = true;
  bool m_verticalAxisVisible   = true;

  /// @variable m_horizontalMin
  /// @variable m_horizontalMax
  /// @variable m_verticalMin
  /// @variable m_verticalMax
  /// @brief  Minimum and maximum values (inclusive) for the coordinate system.
  /// By deafult, center coordinate system at (0, 0) with 1x1 span.
  /// Call setMinMax(int, int, int, int) to change the coordinate system span.
  /// When projectStatementOccurrence(ClintStmtOccurrence *) is called,
  /// the coordinate system is extended so that it accommodates both the origin
  /// point (0, 0) and the projection of statement occurrence.
  int m_horizontalMin = 0;
  int m_horizontalMax = 0;
  int m_verticalMin   = 0;
  int m_verticalMax   = 0;

  QFont m_font;
  QString m_horizontalName;
  QString m_verticalName;

  void updatePolyhedraPositions();
  void addAxisLabels(ClintStmtOccurrence *occurrence);
  void regenerateAxisLabels();
};

#endif // VIZCOORDINATESYSTEM_H
