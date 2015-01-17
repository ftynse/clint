#include "propertiesdialog.h"

#include <QtWidgets>
#include <QtGui>
#include <QtCore>

PropertiesDialog::PropertiesDialog(VizProperties *properties, QWidget *parent) :
  QDialog(parent), m_properties(properties) {

  m_pointRadiusEditor = new QLineEdit;
  m_pointRadiusEditor->setValidator(new QDoubleValidator(1.0, 42.0, 1, this));
  m_coordinateSystemMarginEditor = new QLineEdit;
  m_coordinateSystemMarginEditor->setValidator(new QDoubleValidator(1.0, 42.0, 1, this));
  m_polyhedronOffsetEditor = new QLineEdit;
  m_polyhedronOffsetEditor->setValidator(new QDoubleValidator(0.0, 42.0, 1, this));
  m_pointDistanceEditor = new QLineEdit;
  m_pointDistanceEditor->setValidator(new QDoubleValidator(1.0, 42.0, 1, this));

  m_parameterValueEditor = new QLineEdit;
  m_parameterValueEditor->setValidator(new QIntValidator(1, 42, this));

  QFormLayout *layout = new QFormLayout(this);
  layout->addRow(new QLabel("Point radius"), m_pointRadiusEditor);
  layout->addRow(new QLabel("Point distance"), m_pointDistanceEditor);
  layout->addRow(new QLabel("Polyhedron offset"), m_polyhedronOffsetEditor);
  layout->addRow(new QLabel("Coordinate system margin"), m_coordinateSystemMarginEditor);
  layout->addRow(new QLabel("Parameter value"), m_parameterValueEditor);

  propertyChanged();
  m_parameterValueEditor->setText(QString("6"));

  connect(m_properties, &VizProperties::vizPropertyChanged, this, &PropertiesDialog::propertyChanged);

  connect(m_pointRadiusEditor, &QLineEdit::textChanged, this, &PropertiesDialog::radiusChanged);
  connect(m_pointDistanceEditor, &QLineEdit::textChanged, this, &PropertiesDialog::distanceChanged);
  connect(m_polyhedronOffsetEditor, &QLineEdit::textChanged, this, &PropertiesDialog::offsetChanged);
  connect(m_coordinateSystemMarginEditor, &QLineEdit::textChanged, this, &PropertiesDialog::marginChanged);

  connect(m_parameterValueEditor, &QLineEdit::textChanged, this, &PropertiesDialog::parameterChanged);
}

PropertiesDialog::~PropertiesDialog() {
  disconnect(m_properties, &VizProperties::vizPropertyChanged, this, &PropertiesDialog::propertyChanged);

  disconnect(m_pointRadiusEditor, &QLineEdit::textChanged, this, &PropertiesDialog::radiusChanged);
  disconnect(m_pointDistanceEditor, &QLineEdit::textChanged, this, &PropertiesDialog::distanceChanged);
  disconnect(m_polyhedronOffsetEditor, &QLineEdit::textChanged, this, &PropertiesDialog::offsetChanged);
  disconnect(m_coordinateSystemMarginEditor, &QLineEdit::textChanged, this, &PropertiesDialog::marginChanged);

  disconnect(m_parameterValueEditor, &QLineEdit::textChanged, this, &PropertiesDialog::parameterChanged);
}

void PropertiesDialog::propertyChanged() {
  preventPropagationCycle([this](){
    m_pointDistanceEditor->setText(QString("%1").arg(m_properties->pointDistance()));
    m_pointRadiusEditor->setText(QString("%1").arg(m_properties->pointRadius()));
    m_polyhedronOffsetEditor->setText(QString("%1").arg(m_properties->polyhedronOffset()));
    m_coordinateSystemMarginEditor->setText(QString("%1").arg(m_properties->coordinateSystemMargin()));
  });
}

void PropertiesDialog::radiusChanged() {
  preventPropagationCycle([this](){
    m_properties->setPointRadius(m_pointRadiusEditor->text().toDouble());
  });
}

void PropertiesDialog::distanceChanged() {
  preventPropagationCycle([this](){
    m_properties->setPointDistance(m_pointDistanceEditor->text().toDouble());
  });
}

void PropertiesDialog::offsetChanged() {
  preventPropagationCycle([this](){
    m_properties->setPolyhedronOffset(m_polyhedronOffsetEditor->text().toDouble());
  });
}

void PropertiesDialog::marginChanged() {
  preventPropagationCycle([this](){
    m_properties->setCoordinateSystemMargin(m_coordinateSystemMarginEditor->text().toDouble());
  });
}

void PropertiesDialog::parameterChanged() {
  int value = m_parameterValueEditor->text().toInt();
  emit parameterChange(value);
}
