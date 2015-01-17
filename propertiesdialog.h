#ifndef PROPERTIESDIALOG_H
#define PROPERTIESDIALOG_H

#include "vizproperties.h"

#include <QDialog>
#include <QLineEdit>

class PropertiesDialog : public QDialog {
  Q_OBJECT
public:
  explicit PropertiesDialog(VizProperties *properties, QWidget *parent = 0);
  ~PropertiesDialog();

signals:
  void parameterChange(int value);

public slots:
  void propertyChanged();

  void radiusChanged();
  void distanceChanged();
  void offsetChanged();
  void marginChanged();

  void parameterChanged();

private:
  VizProperties *m_properties;

  QLineEdit *m_pointDistanceEditor,
            *m_pointRadiusEditor,
            *m_coordinateSystemMarginEditor,
            *m_polyhedronOffsetEditor,
            *m_parameterValueEditor;

  bool m_internalChange = false;
  template <typename Func>
  void preventPropagationCycle(Func f) {
    if (m_internalChange)
      return;
    m_internalChange = true;
    f();
    m_internalChange = false;
  }
};

#endif // PROPERTIESDIALOG_H
