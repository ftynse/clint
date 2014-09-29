#ifndef VIZPROGRAM_H
#define VIZPROGRAM_H

#include <QHash>
#include <QObject>
#include <QSet>
#include <QSharedPointer>
#include <QVector>
#include <QWeakPointer>

#include <osl/osl.h>

class VizStatement;
class VizScop;
class VizCoordinateSystem;

class VizProgram : public QObject {
  Q_OBJECT
public:
  explicit VizProgram(osl_scop_p scop, QObject *parent = 0);

  QSet<VizStatement *> statementsInCoordinateSystem(VizCoordinateSystem *system) const {
    return csToStmt_.values(system).toSet();
  }

  QSet<VizCoordinateSystem *> coordinateSystemsForStatement(VizStatement *stmt) const {
    return stmtToCS_.values(stmt).toSet();
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
  QVector<VizScop *> scops_;
};

#endif // VIZPROGRAM_H
