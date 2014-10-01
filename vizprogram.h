#ifndef VIZPROGRAM_H
#define VIZPROGRAM_H

#include <QHash>
#include <QObject>
#include <QSet>
#include <QVector>

#include "enumerator.h"

class VizStatement;
class VizScop;
class VizCoordinateSystem;

class VizProgram : public QObject {
  Q_OBJECT
public:
  explicit VizProgram(osl_scop_p scop, QObject *parent = 0);
  ~VizProgram();

  QSet<VizStatement *> statementsInCoordinateSystem(VizCoordinateSystem *system) const {
    return csToStmt_.values(system).toSet();
  }

  QSet<VizCoordinateSystem *> coordinateSystemsForStatement(VizStatement *stmt) const {
    return stmtToCS_.values(stmt).toSet();
  }

  Enumerator *enumerator() const {
    return m_enumerator;
  }

  VizScop *operator [](int idx) {
    CLINT_ASSERT(idx < m_scops.size(), "Indexed access out of bounds");
    return m_scops.at(idx);
  }

signals:

public slots:

private:
  // coordinate systems and statements should be children of this object
  /** Bidirectional mapping between visual scops and visual coordinates
   *  systems they belong to */
  QMultiHash<VizStatement *, VizCoordinateSystem *> stmtToCS_;
  QMultiHash<VizCoordinateSystem *, VizStatement *> csToStmt_;

  /** A vector of all scops in order of their execution flow */
  QVector<VizScop *> m_scops;

  osl_scop_p m_scop;

  Enumerator *m_enumerator;
};

#endif // VIZPROGRAM_H
