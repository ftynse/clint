#ifndef VIZSCOP_H
#define VIZSCOP_H

#include <QObject>

#include <vizprogram.h>

#include <vector>

class VizStatement;

class VizScop : public QObject
{
  Q_OBJECT
public:
  explicit VizScop(osl_scop_p scop, VizProgram *parent = nullptr);

  // Accessors
  VizProgram *program() const {
    return program_;
  }
signals:

public slots:

private:
  VizProgram *program_;
  std::vector<VizStatement *> statements_;
};

#endif // VIZSCOP_H
