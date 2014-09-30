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
    return m_scop;
  }

  VizProgram *program() const {
    return m_scop->program();
  }

  std::vector<std::vector<int>> projectOn(int horizontalDimIdx, int verticalDimIdx, const std::vector<int> &betaVector) const;

//  QVector<int> scatteredAlphaBeta() const {

//  }
signals:

public slots:

private:
  VizScop *m_scop;
  osl_statement_p m_statement;
};

#endif // VIZSTATEMENT_H
