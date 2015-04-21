#ifndef PROJECTIONVIEW_H
#define PROJECTIONVIEW_H

#include <QGraphicsView>

class ProjectionView : public QGraphicsView {
  Q_OBJECT
public:
  explicit ProjectionView(QWidget *parent = 0);
  explicit ProjectionView(QGraphicsScene *scene, QWidget *parent = 0);

  void setActive(bool active) {
    m_active = active;
  }

  bool isActive() const {
    return m_active;
  }

protected:
  void mousePressEvent(QMouseEvent *event);
  void mouseReleaseEvent(QMouseEvent *event);
  void mouseMoveEvent(QMouseEvent *event);
  void mouseDoubleClickEvent(QMouseEvent *event);

signals:
  void doubleclicked();

private:
  bool m_active = true;
};

#endif // PROJECTIONVIEW_H
