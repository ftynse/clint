#ifndef VIZSTATEMENT_H
#define VIZSTATEMENT_H

#include <QObject>

#include <vizprogram.h>
#include <vizscop.h>

#include <vector>

class VizStatement : public QObject {
  Q_OBJECT
public:
  explicit VizStatement(osl_statement_p stmt, VizScop *parent = nullptr);

  VizScop *scop() const {
    return scop_;
  }

  VizProgram *program() const {
    return scop_->program();
  }

//  QVector<int> scatteredAlphaBeta() const {

//  }
signals:

public slots:

private:
  VizScop *scop_;
};

#endif // VIZSTATEMENT_H
