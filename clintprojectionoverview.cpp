#include "clintprojectionoverview.h"

#include <QtGui>
#include <QtWidgets>
#include <QtSvg>

ClintProjectionOverview::ClintProjectionOverview(ClintScop *cscop, QWidget *parent) : QWidget(parent) {
  resetProjectionMatrix(cscop);
}

void ClintProjectionOverview::resetProjectionMatrix(ClintScop *cscop) {
  for (auto element : m_allProjections) {
    VizProjection *projection = element.second;
    projection->setParent(nullptr);
    delete projection;
  }
  m_allProjections.clear();

  if (m_layout != nullptr) {
    setLayout(nullptr);
    m_layout->setParent(nullptr);
    delete m_layout;
  }

  m_layout = new QGridLayout;
  for (int i = 0, e = cscop->dimensionality(); i < e - 1; i++) {
    for (int j = i + 1; j < e; j++) {
      VizProjection *projection = new VizProjection(i, j, this);
      projection->setViewActive(false);
      projection->projectScop(cscop);
      connect(projection, &VizProjection::selected, this, &ClintProjectionOverview::projectionSelected);
      m_layout->addWidget(projection->widget(), j-1, i);
      m_allProjections[std::make_pair(i, j)] = projection;
    }
  }
  m_layout->setContentsMargins(0, 0, 0, 0);
  setLayout(m_layout);
}

void ClintProjectionOverview::updateAllProjections() {
  for (auto element : m_allProjections) {
    VizProjection *projection = element.second;
    projection->updateProjection();
  }
}

void ClintProjectionOverview::updateProjection(int horizontalDim, int verticalDim) {
  auto iterator = m_allProjections.find(std::make_pair(horizontalDim, verticalDim));
  if (iterator == std::end(m_allProjections)) {
    return;
  }
  iterator->second->updateProjection();
}

VizProperties *ClintProjectionOverview::vizProperties() {
  if (m_allProjections.size() == 0)
    return nullptr;
  auto front = m_allProjections.begin();
  return front->second->vizProperties();
}

void ClintProjectionOverview::fillSvg(QSvgGenerator *generator) {
  QSize totalSize;
  for (auto element : m_allProjections) {
    VizProjection *projection = element.second;
    QSize size = projection->projectionSize();
    totalSize.rwidth() += size.width();
    totalSize.rheight() = qMax(totalSize.height(), size.height());
  }
  generator->setSize(totalSize);
  QPainter *painter = new QPainter(generator);
  for (auto element : m_allProjections) {
    VizProjection *projection = element.second;
    QSize size = projection->projectionSize();
    projection->paintProjection(painter);
    painter->setTransform(QTransform::fromTranslate(size.width(), 0), true);
  }

  delete painter;
}
