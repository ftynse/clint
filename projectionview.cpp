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
  if (m_active)
    QGraphicsView::mousePressEvent(event);
}

void ProjectionView::mouseReleaseEvent(QMouseEvent *event) {
  if (m_active)
    QGraphicsView::mouseReleaseEvent(event);
}

void ProjectionView::mouseMoveEvent(QMouseEvent *event) {
  if (m_active)
    QGraphicsView::mouseMoveEvent(event);
}

void ProjectionView::mouseDoubleClickEvent(QMouseEvent *event) {
  if (m_active)
    QGraphicsView::mouseDoubleClickEvent(event);
  emit doubleclicked();
}
