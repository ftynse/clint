#include <QtCore>
#include <QtGui>
#include <QtWidgets>
#include <QtSvg>

#include "vizprojection.h"
#include "clintwindow.h"
#include "oslutils.h"

void ClintWindow::resetCentralWidget(QWidget *interface) {
  if (centralWidget() != nullptr) {
    QWidget *oldWidget = centralWidget();
    m_scriptEditor->setParent(nullptr);
    m_codeEditor->setParent(nullptr);
    delete oldWidget;
  }

  if (interface == nullptr) {
    setCentralWidget(nullptr);
    return;
  }

  QLabel *TODOupdater = new QLabel("<\n>");
  QGridLayout *topLayout = new QGridLayout;
  topLayout->addWidget(interface, 0, 0, 2, 1 /*,Qt::AlignCenter | Qt::AlignVCenter*/);
  topLayout->addWidget(TODOupdater, 0, 1, 2, 1 /*,Qt::AlignVCenter*/);
  topLayout->addWidget(m_scriptEditor, 0, 2, 1, 1 /*,Qt::AlignCenter | Qt::AlignTop*/);
  topLayout->addWidget(m_codeEditor, 1, 2, 1, 1  /*,Qt::AlignCenter| Qt::AlignBottom*/);

  topLayout->setColumnStretch(0, 3);
  topLayout->setColumnStretch(2, 1);
  topLayout->setRowStretch(0, 1);
  topLayout->setRowStretch(1, 3);

//  QBoxLayout *topLayout = new QBoxLayout(QBoxLayout::LeftToRight);
//  topLayout->addWidget(interface, 3);
//  topLayout->addWidget(TODOupdater, 0);
//  QBoxLayout *innerLayout = new QBoxLayout(QBoxLayout::TopToBottom);
//  innerLayout->addWidget(scriptEditor, 1);
//  innerLayout->addWidget(codeEditor, 3);
//  topLayout->addLayout(innerLayout, 1);

  QWidget *topWidget = new QWidget;
  topWidget->setLayout(topLayout);
  setCentralWidget(topWidget);
}

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

  QFont monospacefont("PT Mono");
  m_scriptEditor = new QTextEdit;
  m_codeEditor = new QTextEdit;
  m_codeEditor->setFont(monospacefont);
  m_scriptEditor->setFont(monospacefont);

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
  m_actionFileSaveSvg = new QAction("Save as SVG", this);
  m_actionFileQuit = new QAction(QIcon::fromTheme("application-exit"), "Quit", this);

  m_actionFileOpen->setShortcut(QKeySequence::Open);
  m_actionFileClose->setShortcut(QKeySequence::Close);
  m_actionFileQuit->setShortcut(QKeySequence::Quit);

  m_actionFileClose->setEnabled(false);

  m_actionEditUndo = new QAction(QIcon::fromTheme("edit-undo"), "Undo", this);
  m_actionEditRedo = new QAction(QIcon::fromTheme("edit-redo"), "Redo", this);

  m_actionEditUndo->setShortcut(QKeySequence::Undo);
  m_actionEditRedo->setShortcut(QKeySequence::Redo);

  m_actionEditUndo->setEnabled(false);
  m_actionEditRedo->setEnabled(false);

  m_actionViewFreeze = new QAction("Keep original code", this);
  m_actionViewFreeze->setCheckable(true);

  connect(m_actionFileOpen, &QAction::triggered, this, &ClintWindow::fileOpen);
  connect(m_actionFileClose, &QAction::triggered, this, &ClintWindow::fileClose);
  connect(m_actionFileSaveSvg, &QAction::triggered, this, &ClintWindow::fileSaveSvg);
  connect(m_actionFileQuit, &QAction::triggered, qApp, &QApplication::quit);

  connect(m_actionEditUndo, &QAction::triggered, this, &ClintWindow::editUndo);
  connect(m_actionEditRedo, &QAction::triggered, this, &ClintWindow::editRedo);

  connect(m_actionViewFreeze, &QAction::toggled, this, &ClintWindow::viewFreezeToggled);
}

void ClintWindow::setupMenus() {
  m_menuBar = new QMenuBar(this);
  QMenu *fileMenu = new QMenu("File");
  fileMenu->addAction(m_actionFileOpen);
  fileMenu->addAction(m_actionFileSaveSvg);
  fileMenu->addAction(m_actionFileClose);
  fileMenu->addSeparator();
  fileMenu->addAction(m_actionFileQuit);

  QMenu *editMenu = new QMenu("Edit");
  editMenu->addAction(m_actionEditUndo);
  editMenu->addAction(m_actionEditRedo);

  QMenu *viewMenu = new QMenu("View");
  viewMenu->addAction(m_actionViewFreeze);

  m_menuBar->addAction(fileMenu->menuAction());
  m_menuBar->addAction(editMenu->menuAction());
  m_menuBar->addAction(viewMenu->menuAction());
  m_menuBar->setNativeMenuBar(false);  // Override MacOS behavior since it does not display the menu

  setMenuBar(m_menuBar);
}

void ClintWindow::fileOpen() {
  QString selectedFilter;
  QString fileName = QFileDialog::getOpenFileName(this, "Open file", QString(), "OpenScop files (*.scop);;C/C++ sources (*.c *.cpp *.cxx)", &selectedFilter);
  if (fileName.isNull())
    return;

  if (m_fileOpen) {
    fileClose();
  }

  openFileByName(fileName);
}

void ClintWindow::fileClose() {
  if (!m_fileOpen)
    return;

  if (m_program) {
    ClintScop *vscop = (*m_program)[0];
    if (vscop)
      disconnect(vscop, &ClintScop::transformExecuted, this, &ClintWindow::scopTransformed);
  }

  setWindowTitle("Clint: Chunky Loop INTerface");

  resetCentralWidget(nullptr);
  m_program->setParent(nullptr);
  delete m_program;
  m_program = nullptr;
  m_projection->setParent(nullptr);
  delete m_projection;
  m_projection = nullptr;

  m_fileOpen = false;
  m_actionFileClose->setEnabled(false);
}

void ClintWindow::fileSaveSvg() {
  if (!m_fileOpen)
    return;

  QString fileName = QFileDialog::getSaveFileName(this, "Save SVG image", QString(), "Scalable Vector Graphics (*.svg)");
  if (fileName.isNull())
    return;

  QSvgGenerator *generator = new QSvgGenerator;
  generator->setFileName(fileName);
  generator->setSize(m_projection->projectionSize());
  QPainter *painter = new QPainter(generator);
  m_projection->paintProjection(painter);

  delete painter;
  delete generator;
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
  char *originalCode = nullptr;
  if (fileName.endsWith(".scop")) {
    scop = osl_scop_read(file);
  } else if (fileName.endsWith(".c") ||
             fileName.endsWith(".cpp") ||
             fileName.endsWith(".cxx")) {
    scop = oslFromCCode(file);
    originalCode = fileContents(file);
  } else {
    CLINT_UNREACHABLE;
  }
  fclose(file);
  if (!scop) {
    QMessageBox::warning(this, QString(), "No SCoP in the given file", QMessageBox::Ok, QMessageBox::Ok);
    return;
  }

  setWindowTitle(QString("%1 - Clint").arg(fileNameNoPath));

  m_program = new ClintProgram(scop, originalCode, this);
  ClintScop *vscop = (*m_program)[0];
  connect(vscop, &ClintScop::transformExecuted, this, &ClintWindow::scopTransformed);
  m_projection = new VizProjection(0, 1, this);
  m_projection->projectScop(vscop);

  m_codeEditor->setHtml(vscop->originalHtml());
  m_scriptEditor->setHtml(QString());

  resetCentralWidget(m_projection->widget());

  m_fileOpen = true;
  m_actionFileClose->setEnabled(true);

  if (originalCode)
    free(originalCode);
}

ClintScop *ClintWindow::regenerateScop(ClintScop *vscop) {
  osl_scop_p scop = vscop->appliedScop();
  ClintScop *newscop = new ClintScop(scop, nullptr, m_program);
  m_projection->projectScop(newscop);
  for (TransformationGroup g : vscop->transformationSequence().groups) {
    newscop->transform(g);
  }
  newscop->resetRedoSequence(vscop->redoSequence());
  newscop->setScopSilent(vscop->scopPart());
  (*m_program)[0] = newscop;
  disconnect(vscop, &ClintScop::transformExecuted, this, &ClintWindow::scopTransformed);
  connect(newscop, &ClintScop::transformExecuted, this, &ClintWindow::scopTransformed);

  updateCodeEditor();
  m_scriptEditor->setText(newscop->currentScript());

  return newscop;
}

void ClintWindow::editUndo() {

  if (!m_program)
    return;
  ClintScop *vscop = (*m_program)[0];
  if (!vscop)
    return;
  CLINT_ASSERT(vscop->hasUndo(), "No undo possible, but the button is enabled");
  vscop->undoTransformation();

  ClintScop *newscop = regenerateScop(vscop);
  delete vscop;

  m_actionEditUndo->setEnabled(newscop->hasUndo());
  m_actionEditRedo->setEnabled(true);
}

void ClintWindow::editRedo() {

  if (!m_program)
    return;
  ClintScop *vscop = (*m_program)[0];
  if (!vscop)
    return;
  CLINT_ASSERT(vscop->hasRedo(), "No redo possible, but the button is enabled");
  vscop->redoTransformation();

  ClintScop *newscop = regenerateScop(vscop);
  delete vscop;

  m_actionEditRedo->setEnabled(newscop->hasRedo());
  m_actionEditUndo->setEnabled(true);
}

void ClintWindow::viewFreezeToggled(bool value) {
  m_showOriginalCode = value;
  scopTransformed();
}

void ClintWindow::updateCodeEditor() {
  if (!m_program)
    return;
  ClintScop *vscop = (*m_program)[0];
  if (!vscop)
    return;

  if (!m_showOriginalCode) {
//    codeEditor->setText(QString(vscop->generatedCode()));
    m_codeEditor->setHtml(QString(vscop->generatedHtml()));
  } else {
//    codeEditor->setText(QString(vscop->originalCode()));
    m_codeEditor->setHtml(QString(vscop->originalHtml()));
  }
}

void ClintWindow::scopTransformed() {
  if (!m_program)
    return;
  ClintScop *vscop = (*m_program)[0];
  if (!vscop)
    return;

  updateCodeEditor();
  m_scriptEditor->setText(QString(vscop->currentScript()));

  if (vscop->hasRedo())
    vscop->clearRedo();

  m_actionEditUndo->setEnabled(vscop->hasUndo());
  m_actionEditRedo->setEnabled(false);
}
