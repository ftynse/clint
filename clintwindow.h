#ifndef CLINTWINDOW_H
#define CLINTWINDOW_H

#include <QtWidgets>
#include <QMainWindow>

#include "clintprogram.h"
#include "vizprojection.h"

class QWidget;
class QTextEdit;

class ClintWindow : public QMainWindow {
  Q_OBJECT
public:
  explicit ClintWindow(QWidget *parent = 0);

    ClintScop * regenerateScop(ClintScop *vscop);
signals:

public slots:
  void fileOpen();
  void fileClose();
  void fileSaveSvg();
  void openFileByName(QString fileName);

  void editUndo();
  void editRedo();

  void viewFreezeToggled(bool value);

  void scopTransformed();

  void updateCodeEditor();

private:
  QAction *m_actionFileOpen;
  QAction *m_actionFileClose;
  QAction *m_actionFileSaveSvg;
  QAction *m_actionFileQuit;

  QAction *m_actionEditUndo;
  QAction *m_actionEditRedo;

  QAction *m_actionViewFreeze;

  QMenuBar *m_menuBar;

  bool m_fileOpen = false;

  ClintProgram *m_program = nullptr;
  VizProjection *m_projection = nullptr;
  QTextEdit *m_codeEditor = nullptr;
  QTextEdit *m_scriptEditor = nullptr;

  bool m_showOriginalCode = false;

  void setupActions();
  void setupMenus();

  void resetCentralWidget(QWidget *interface = nullptr);
};

#endif // CLINTWINDOW_H
