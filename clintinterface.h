#ifndef CLINTINTERFACE_H
#define CLINTINTERFACE_H

#include <QWidget>
#include <QList>

#include "clintprojection.h"

class ClintInterface : public QWidget
{
  Q_OBJECT
public:
  explicit ClintInterface(QWidget *parent = 0);

signals:

public slots:

private:
  // Interface is a list of projections
  QList<ClintProjection *> projections_;
};

#endif // CLINTINTERFACE_H
