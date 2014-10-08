#include <QtGui>
#include <QtWidgets>

#include "vizprojection.h"
#include "clintwindow.h"

namespace {

QWidget *clintInterfaceMockup() {
  QGridLayout *topLayout = new QGridLayout;

  QLabel *TODOvisualization = new QLabel("TODO: visualization");
  QLabel *TODOupdater = new QLabel("<\n>");
  QListView *TODOhistory = new QListView();
  QTextEdit *TODOcodeEditor = new QTextEdit();

  topLayout->addWidget(TODOvisualization, 0, 0, 2, 1, Qt::AlignCenter | Qt::AlignVCenter);
  topLayout->addWidget(TODOupdater, 0, 1, 2, 1, Qt::AlignVCenter);
  topLayout->addWidget(TODOhistory, 0, 2, 1, 1, Qt::AlignCenter | Qt::AlignTop);
  topLayout->addWidget(TODOcodeEditor, 1, 2, 1, 1, Qt::AlignCenter | Qt::AlignBottom);

  topLayout->setColumnStretch(0, 3);
  topLayout->setColumnStretch(2, 1);

  QWidget *topWidget = new QWidget;
  topWidget->setLayout(topLayout);
  return topWidget;
}

} // end anonymous namespace

ClintWindow::ClintWindow(QWidget *parent) :
  QMainWindow(parent) {

  setWindowTitle("Clint: Chunky Loop INTerface");

  VizProjection *projection = new VizProjection(0, 1, this);

  FILE *f = fopen("maxviz.scop", "r");
  if (!f) fprintf(stderr, "blah\n");
  osl_scop_p scop = osl_scop_read(f);
  fclose(f);

  ClintProgram *vprogram = new ClintProgram(scop, this);
  ClintScop *vscop = (*vprogram)[0];
  projection->projectScop(vscop);

  setCentralWidget(projection->widget());
}
