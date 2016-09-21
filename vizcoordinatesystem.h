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
class VizDepArrow;

class VizCoordinateSystem : public QGraphicsObject {
  Q_OBJECT
public:
  enum class AxisState {
    Invisible,
    Visible,
    WillAppear,
    WillDisappear
  };

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
    return m_horizontalAxisState == AxisState::Visible;
  }

  bool isVerticalAxisVisible() const {
    return m_verticalAxisState == AxisState::Visible;
  }

  void setHorizontalAxisState(VizCoordinateSystem::AxisState state) {
    bool scheduleRepaint = (state != m_horizontalAxisState);
    m_horizontalAxisState = state;
    if (scheduleRepaint) {
      update();
    }
  }

  void setVerticalAxisState(VizCoordinateSystem::AxisState state) {
    bool scheduleRepaint = (state != m_verticalAxisState);
    m_verticalAxisState = state;
    if (scheduleRepaint) {
      update();
    }
  }

  size_t horizontalDimensionIdx() const {
    return m_horizontalDimensionIdx;
  }

  size_t verticalDimensionIdx() const {
    return m_verticalDimensionIdx;
  }

  void setHorizontalDimensionIdx(size_t dimIdx);
  void setVerticalDimensionIdx(size_t dimIdx);

  QRectF coordinateSystemRect() const;
  QRectF outerRect() const;

  bool projectStatementOccurrence(ClintStmtOccurrence *occurrence);
  void updateInnerDependences();
  void updateInternalDependences();
  void setInnerDependencesBetween(VizPolyhedron *vp1, VizPolyhedron *vp2, std::vector<std::vector<int>> &&lines, bool violated);

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
  void setHorizontalMinMax(int horizontalMinimum, int horizontalMaximum);
  void setVerticalMinMax(int verticalMinimum, int verticalMaximum);


  std::pair<int, int> horizontalMinMax() const {
    return std::make_pair(m_horizontalMin, m_horizontalMax);
  }
  std::pair<int, int> verticalMinMax() const {
    return std::make_pair(m_verticalMin, m_verticalMax);
  }

  void setMinMax(const std::pair<int, int> &horizontal,
                 const std::pair<int, int> &vertical) {
    setMinMax(horizontal.first, horizontal.second,
              vertical.first, vertical.second);
  }
  void setHorizontalMinMax(const std::pair<int, int> &horizontal) {
    setHorizontalMinMax(horizontal.first, horizontal.second);
  }
  void setVerticalMinMax(const std::pair<int, int> &vertical) {
    setVerticalMinMax(vertical.first, vertical.second);
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

  VizPolyhedron *polyhedron(const std::vector<int> &beta) const;
  const std::vector<VizPolyhedron *> &polyhedra() const;
  VizPolyhedron *shadow(VizPolyhedron *original) const;
  VizPolyhedron *animationTarget(VizPolyhedron *original) const;

  std::vector<int> betaPrefix() const;

  int dependentWith(VizCoordinateSystem *vcs);
  bool nextCsIsDependent = false;
  bool nextPileIsDependent = false;
  bool nextCsIsViolated = false;
  bool nextPileIsViolated = false;

  int horizontalAxisLength() const;
  int verticalAxisLength() const;
  int ticMargin() const;

  void polyhedronUpdated(VizPolyhedron *polyhedron);
  void setPolyhedronCoordinates(VizPolyhedron *polyhedron, int horizontal, int vertical,
                                bool ignoreHorizontal = false, bool ignoreVertical = false);
  void reparentPolyhedron(VizPolyhedron *polyhedron);
  void resetPolyhedronPos(VizPolyhedron *polyhedron);

  void createPolyhedronShadow(VizPolyhedron *polyhedron);
  void createPolyhedronAnimationTarget(VizPolyhedron *polyhedron);
  void clearPolyhedronShadows();
  void clearPolyhedronAnimationTargets();

  void insertPolyhedronAfter(VizPolyhedron *inserted, VizPolyhedron *after);
  void removePolyhedron(VizPolyhedron *polyhedron);
  void updateAllPositions();
  void setIgnorePolyhedraPositionUpdates(bool ignore = true);

  void setHighlightTarget(bool value = true) {
    m_isHighlightedTarget = value;
    update();
  }

  void deleteInnerDependences();

  void reorderPolyhedra(const Transformation &transformation);

signals:

public slots:
  void finalizeOccurrenceChange();

private:
  std::vector<VizPolyhedron *> m_polyhedra;
  std::unordered_map<size_t, VizPolyhedron *> m_polyhedronShadows;
  std::unordered_map<VizPolyhedron *, VizPolyhedron *> m_polyhedronAnimationTargets;
  ClintProgram *m_program;
  VizProjection *m_projection;
  std::unordered_set<VizDepArrow *> m_depArrows;

  size_t m_horizontalDimensionIdx = VizProperties::UNDEFINED_DIMENSION;
  size_t m_verticalDimensionIdx   = VizProperties::UNDEFINED_DIMENSION;

  AxisState m_horizontalAxisState = AxisState::Visible;
  AxisState m_verticalAxisState   = AxisState::Visible;

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

  bool m_ignorePolyhedraPositionUpdates = false;
  bool m_isHighlightedTarget = false;

  void addAxisLabels(ClintStmtOccurrence *occurrence);
  void regenerateAxisLabels();
  void updatePolyhedraPositions();
  void updatePolyhedraPositionsAnimated();
  QPolygonF leftArrow(int length, const double pointRadius);
  QPolygonF topArrow(int length, const double pointRadius);

  QPointF polyhedronPosition(VizPolyhedron *polyhedron, size_t idx,
                             bool ignoreHorizontal = false, bool ignoreVertical = false);

};

#endif // VIZCOORDINATESYSTEM_H
