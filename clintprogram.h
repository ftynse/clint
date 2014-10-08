#ifndef CLINTPROGRAM_H
#define CLINTPROGRAM_H

#include <QHash>
#include <QObject>
#include <QSet>
#include <QVector>

#include "enumerator.h"

class ClintStmt;
class ClintScop;
class VizCoordinateSystem;

class ClintProgram : public QObject {
  Q_OBJECT
public:
  explicit ClintProgram(osl_scop_p scop, QObject *parent = 0);
  ~ClintProgram();

  QSet<ClintStmt *> statementsInCoordinateSystem(VizCoordinateSystem *system) const {
    return csToStmt_.values(system).toSet();
  }

  QSet<VizCoordinateSystem *> coordinateSystemsForStatement(ClintStmt *stmt) const {
    return stmtToCS_.values(stmt).toSet();
  }

  Enumerator *enumerator() const {
    return m_enumerator;
  }

  ClintScop *operator [](int idx) {
    CLINT_ASSERT(idx < m_scops.size(), "Indexed access out of bounds");
    return m_scops.at(idx);
  }

signals:

public slots:

private:
  // coordinate systems and statements should be children of this object
  /** Bidirectional mapping between visual scops and visual coordinates
   *  systems they belong to */
  QMultiHash<ClintStmt *, VizCoordinateSystem *> stmtToCS_;
  QMultiHash<VizCoordinateSystem *, ClintStmt *> csToStmt_;

  /** A vector of all scops in order of their execution flow */
  QVector<ClintScop *> m_scops;

  osl_scop_p m_scop;

  Enumerator *m_enumerator;
};

#endif // CLINTPROGRAM_H
