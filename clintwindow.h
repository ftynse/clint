#ifndef CLINTWINDOW_H
#define CLINTWINDOW_H

#include <QtWidgets>
#include <QMainWindow>

#include "clintprogram.h"
#include "vizprojection.h"

class ClintWindow : public QMainWindow
{
  Q_OBJECT
public:
  explicit ClintWindow(QWidget *parent = 0);

signals:

public slots:
  void fileOpen();
  void fileClose();
  void openFileByName(QString fileName);

private:
  QAction *m_actionFileOpen;
  QAction *m_actionFileClose;
  QAction *m_actionFileQuit;
  QMenuBar *m_menuBar;

  bool m_fileOpen = false;

  ClintProgram *m_program = nullptr;
  VizProjection *m_projection = nullptr;

  void setupActions();
  void setupMenus();
};

#endif // CLINTWINDOW_H
