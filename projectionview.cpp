#include <QtGui>
#include <QtWidgets>

#include "projectionview.h"

ProjectionView::ProjectionView(QWidget *parent) :
  QGraphicsView(parent) {
}

ProjectionView::ProjectionView(QGraphicsScene *scene, QWidget *parent) :
  QGraphicsView(scene, parent) {
}

void ProjectionView::mousePressEvent(QMouseEvent *event) {
  event->accept();
}

void ProjectionView::mouseReleaseEvent(QMouseEvent *event) {
  event->accept();
}

void ProjectionView::mouseMoveEvent(QMouseEvent *event) {
  event->accept();
}

void ProjectionView::mouseDoubleClickEvent(QMouseEvent *event) {
  emit doubleclicked();
  event->accept();
}
