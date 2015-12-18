#ifndef CLINTWINDOW_H
#define CLINTWINDOW_H

#include <QtWidgets>
#include <QMainWindow>

#include "clintprogram.h"
#include "clintprojectionoverview.h"
#include "vizprojection.h"
#include "vizproperties.h"

class QWidget;
class QTextEdit;

class ClintWindow : public QMainWindow {
  Q_OBJECT
public:
  explicit ClintWindow(QWidget *parent = 0);
  ~ClintWindow();

  void regenerateScop(osl_scop_p originalScop = nullptr);
  void regenerateScop(const TransformationSequence &sequence);
  void createProjections(ClintScop *vscop);
  void paintTogether(QPainter *painter, QSvgGenerator *generator);

signals:

public slots:
  void fileOpen();
  void fileClose();
  void fileSaveSvg();
  void openFileByName(QString fileName);

  void editUndo();
  void editRedo();
  void editVizProperties();

  void viewFreezeToggled(bool value);
  void viewProjectionMatrixToggled(bool value);

  void scopTransformed();

  void updateCodeEditor();
  void reparseCode();
  void reparseScript();

  void changeParameter(int value);
  void projectionSelectedInOverview(int horizontal, int vertical);
  void projectionSelectedAlone(int horizontal, int vertical);

private:
  QAction *m_actionFileOpen;
  QAction *m_actionFileClose;
  QAction *m_actionFileSaveSvg;
  QAction *m_actionFileQuit;

  QAction *m_actionEditUndo;
  QAction *m_actionEditRedo;
  QAction *m_actionEditVizProperties;

  QAction *m_actionViewFreeze;
  QAction *m_actionViewProjectionMatrix;

  QMenuBar *m_menuBar;

  bool m_fileOpen = false;

  ClintProgram *m_program = nullptr;
  VizProjection *m_projection = nullptr;
  ClintProjectionOverview *m_projectionOverview = nullptr;
  QWidget *m_graphicalInterface = nullptr;
  QTextEdit *m_codeEditor = nullptr;
  QTextEdit *m_scriptEditor = nullptr;
  QPushButton *m_reparseCodeButton = nullptr,
              *m_reparseScriptButton = nullptr;

  bool m_showOriginalCode = false;
  int m_parameterValue = 6;
  size_t m_horizontalDimSelected = VizProperties::NO_DIMENSION,
         m_verticalDimSelected = VizProperties::NO_DIMENSION;
  QString m_fileBasename;

  bool m_undoing = false;

  void setupActions();
  void setupMenus();

  void resetCentralWidget(QWidget *interface = nullptr);
  ClintScop *regenerateScopWithSequence(osl_scop_p originalScop, const TransformationSequence &sequence);
  void deleteProjectionOverview();
  void deleteProjection();
};

#endif // CLINTWINDOW_H
