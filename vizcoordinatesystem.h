#ifndef VIZCOORDINATESYSTEM_H
#define VIZCOORDINATESYSTEM_H

#include <QObject>
#include <QGraphicsObject>
#include <QSet>

#include <vizprogram.h>
#include <vizscop.h>
#include <vizstatement.h>

#define DISPLACEMENT 4
#define CLINT_UNDEFINED -1ULL

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

  bool addStatement__(osl_scop_p scop, osl_statement_p stmt, const std::vector<int> &betaVector);
  bool projectStatement(VizStatement *statement, const std::vector<int> &betaVector);

signals:

public slots:

private:
  std::vector<VizPolyhedron *> m_polyhedra;
  VizProgram *m_program;

  size_t m_horizontalDimensionIdx = CLINT_UNDEFINED;
  size_t m_verticalDimensionIdx   = CLINT_UNDEFINED;

  bool m_horizontalAxisVisible = true;
  bool m_verticalAxisVisible   = true;
};

#endif // VIZCOORDINATESYSTEM_H
