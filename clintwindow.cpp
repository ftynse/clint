#include <QtCore>
#include <QtGui>
#include <QtWidgets>

#include "vizprojection.h"
#include "clintwindow.h"
#include "oslutils.h"

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

  QString filename;
  QStringList args = qApp->arguments();
  for (int i = 0; i < args.size(); i++) {
    if (!args[i].startsWith("--") && !args[i].endsWith("clint")) {
      if (filename.length() != 0) {
        QMessageBox::warning(this, "Multiple files to open", "Extra file to open is ignored", QMessageBox::Ok, QMessageBox::Ok);
        continue;
      }
      filename = args[i];
    }
  }

  setWindowTitle("Clint: Chunky Loop INTerface");
  setupActions();
  setupMenus();
  if (filename.length() != 0) {
    openFileByName(filename);
  }
}

void ClintWindow::setupActions() {
  m_actionFileOpen = new QAction(QIcon::fromTheme("document-open"), "Open...", this);
  m_actionFileClose = new QAction("Close", this);
  m_actionFileQuit = new QAction(QIcon::fromTheme("application-exit"), "Quit", this);

  m_actionFileOpen->setShortcut(QKeySequence::Open);
  m_actionFileClose->setShortcut(QKeySequence::Close);
  m_actionFileQuit->setShortcut(QKeySequence::Quit);

  m_actionFileClose->setEnabled(false);

  connect(m_actionFileOpen, &QAction::triggered, this, &ClintWindow::fileOpen);
  connect(m_actionFileClose, &QAction::triggered, this, &ClintWindow::fileClose);
  connect(m_actionFileQuit, &QAction::triggered, qApp, &QApplication::quit);
}

void ClintWindow::setupMenus() {
  m_menuBar = new QMenuBar(this);
  QMenu *fileMenu = new QMenu("File");
  fileMenu->addAction(m_actionFileOpen);
  fileMenu->addAction(m_actionFileClose);
  fileMenu->addSeparator();
  fileMenu->addAction(m_actionFileQuit);
  m_menuBar->addAction(fileMenu->menuAction());
  m_menuBar->setNativeMenuBar(false);  // Override MacOS behavior since it does not display the menu
  setMenuBar(m_menuBar);
}

void ClintWindow::fileOpen() {
  if (m_fileOpen) {
    fileClose();
  }

  QString selectedFilter;
  QString fileName = QFileDialog::getOpenFileName(this, "Open file", QString(), "OpenScop files (*.scop);;C/C++ sources (*.c *.cpp *.cxx)", &selectedFilter);
  if (fileName.isNull())
    return;
  openFileByName(fileName);
}

void ClintWindow::fileClose() {
  if (!m_fileOpen)
    return;

  setWindowTitle("Clint: Chunky Loop INTerface");

  setCentralWidget(nullptr);
  m_program->setParent(nullptr);
  delete m_program;
  m_program = nullptr;
  m_projection->setParent(nullptr);
  delete m_projection;
  m_projection = nullptr;

  m_fileOpen = false;
  m_actionFileClose->setEnabled(false);
}

void ClintWindow::openFileByName(QString fileName) {
  char *cFileName = strdup(QFile::encodeName(fileName).constData());
  QString fileNameNoPath = QFileInfo(fileName).fileName();
  FILE *file = fopen(cFileName, "r");
  free(cFileName);
  if (!file) {
    QMessageBox::critical(this, QString(), QString("Could not open %1 for reading").arg(fileNameNoPath), QMessageBox::Ok, QMessageBox::Ok);
    return;
  }

  osl_scop_p scop = nullptr;
  if (fileName.endsWith(".scop")) {
    scop = osl_scop_read(file);
  } else if (fileName.endsWith(".c") ||
             fileName.endsWith(".cpp") ||
             fileName.endsWith(".cxx")) {
    scop = oslFromCCode(file);
  } else {
    CLINT_UNREACHABLE;
  }
  fclose(file);
  if (!scop) {
    QMessageBox::warning(this, QString(), "No SCoP in the given file", QMessageBox::Ok, QMessageBox::Ok);
    return;
  }

  setWindowTitle(QString("%1 - Clint").arg(fileNameNoPath));

  m_program = new ClintProgram(scop, this);
  ClintScop *vscop = (*m_program)[0];
  m_projection = new VizProjection(0, 1, this);
  m_projection->projectScop(vscop);

  setCentralWidget(m_projection->widget());

  m_fileOpen = true;
  m_actionFileClose->setEnabled(true);
}
