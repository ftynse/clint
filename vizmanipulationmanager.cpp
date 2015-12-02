#include "vizcoordinatesystem.h"
#include "vizmanipulationmanager.h"
#include "vizpoint.h"
#include "vizpolyhedron.h"
#include "vizproperties.h"
#include "vizprojection.h"
#include "vizselectionmanager.h"

#include <QtGui>
#include <QtWidgets>

#include <set>
#include <map>

VizManipulationManager::VizManipulationManager(QObject *parent) :
  QObject(parent) {
}

void VizManipulationManager::ensureTargetConsistency() {
  int sum = 0;
  sum += (m_polyhedron != nullptr);
  sum += (m_point != nullptr);
  sum += (m_coordinateSystem != nullptr);
  // Should not be possible unless we are in the multitouch environment.
  CLINT_ASSERT(sum == 1, "More than one active movable object manipulated at the same time");
}

void VizManipulationManager::polyhedronAboutToMove(VizPolyhedron *polyhedron) {
  CLINT_ASSERT(m_polyhedron == nullptr, "Active polyhedron is already being manipulated");
  m_polyhedron = polyhedron;
  ensureTargetConsistency();

  m_horzOffset = 0;
  m_vertOffset = 0;
  m_firstMovement = true;

  int hmax, vmax;
  polyhedron->coordinateSystem()->minMax(m_initCSHorizontalMin, hmax, m_initCSVerticalMin, vmax);
}

void VizManipulationManager::polyhedronMoving(VizPolyhedron *polyhedron, QPointF displacement) {
  CLINT_ASSERT(polyhedron == m_polyhedron, "Another polyhedron is being manipulated");

  if (m_firstMovement) {
    m_firstMovement = false;
    const std::unordered_set<VizPolyhedron *> &selectedPolyhedra =
        polyhedron->coordinateSystem()->projection()->selectionManager()->selectedPolyhedra();
    for (VizPolyhedron *vp : selectedPolyhedra) {
      vp->setOverrideSetPos(true);
    }
  }

  VizCoordinateSystem *cs = polyhedron->coordinateSystem();
  const VizProperties *properties = cs->projection()->vizProperties();
  double absoluteDistanceX = displacement.x() / properties->pointDistance();
  double absoluteDistanceY = -displacement.y() / properties->pointDistance(); // Inverted vertical axis
  int horzOffset = static_cast<int>(round(absoluteDistanceX));
  int vertOffset = static_cast<int>(round(absoluteDistanceY));
  if (horzOffset != m_horzOffset || vertOffset != m_vertOffset) {
    // Get selected objects.  (Should be in the same coordinate system.)
    const std::unordered_set<VizPolyhedron *> &selectedPolyhedra =
        cs->projection()->selectionManager()->selectedPolyhedra();
    CLINT_ASSERT(std::find(std::begin(selectedPolyhedra), std::end(selectedPolyhedra), polyhedron) != std::end(selectedPolyhedra),
                 "The active polyhedra is not selected");
    int phHmin = INT_MAX, phHmax = INT_MIN, phVmin = INT_MAX, phVmax = INT_MIN;
    for (VizPolyhedron *ph : selectedPolyhedra) {
      phHmin = std::min(phHmin, ph->localHorizontalMin());
      phHmax = std::max(phHmax, ph->localHorizontalMax());
      phVmin = std::min(phVmin, ph->localVerticalMin());
      phVmax = std::max(phVmax, ph->localVerticalMax());
    }

    if (horzOffset != m_horzOffset) {
      int hmin = phHmin + horzOffset;
      int hmax = phHmax + horzOffset;
      cs->projection()->ensureFitsHorizontally(cs, hmin, hmax);
      emit intentionMoveHorizontally(horzOffset - m_horzOffset);
      m_horzOffset = horzOffset;
    }

    if (vertOffset != m_vertOffset) {
      int vmin = phVmin + vertOffset;
      int vmax = phVmax + vertOffset;
      cs->projection()->ensureFitsVertically(cs, vmin, vmax);
      emit intentionMoveVertically(vertOffset - m_vertOffset);
      m_vertOffset = vertOffset;
    }
  }
}

void VizManipulationManager::polyhedronHasMoved(VizPolyhedron *polyhedron) {
  CLINT_ASSERT(m_polyhedron == polyhedron, "Signaled end of polyhedron movement that was never initiated");
  m_polyhedron = nullptr;
  m_firstMovement = false;
  TransformationGroup group;
  const std::unordered_set<VizPolyhedron *> &selectedPolyhedra =
      polyhedron->coordinateSystem()->projection()->selectionManager()->selectedPolyhedra();
  for (VizPolyhedron *vp : selectedPolyhedra) {
    vp->setOverrideSetPos(false);
  }
  if (m_horzOffset != 0 || m_vertOffset != 0) {
    // TODO: move this code to transformation manager
    // we need transformation manager to keep three different views in sync
    CLINT_ASSERT(std::find(std::begin(selectedPolyhedra), std::end(selectedPolyhedra), polyhedron) != std::end(selectedPolyhedra),
                 "The active polyhedra is not selected");

    for (VizPolyhedron *vp : selectedPolyhedra) {
      const std::vector<int> beta = vp->occurrence()->betaVector();
      size_t horzDepth = vp->coordinateSystem()->horizontalDimensionIdx() == VizProperties::NO_DIMENSION ?
            VizProperties::NO_DIMENSION :
            vp->occurrence()->depth(vp->coordinateSystem()->horizontalDimensionIdx());
      int horzAmount = m_horzOffset;
      size_t vertDepth = vp->coordinateSystem()->verticalDimensionIdx() == VizProperties::NO_DIMENSION ?
            VizProperties::NO_DIMENSION :
            vp->occurrence()->depth(vp->coordinateSystem()->verticalDimensionIdx());
      int vertAmount = m_vertOffset;
      bool oneDimensional = vp->occurrence()->dimensionality() < vp->coordinateSystem()->verticalDimensionIdx();
      bool zeroDimensional = vp->occurrence()->dimensionality() < vp->coordinateSystem()->horizontalDimensionIdx();
      if (!zeroDimensional && m_horzOffset != 0) {
        Transformation transformation = Transformation::constantShift(beta, horzDepth, -horzAmount);
        group.transformations.push_back(transformation);
      }
      if (!oneDimensional && m_vertOffset != 0) {
        Transformation transformation = Transformation::constantShift(beta, vertDepth, -vertAmount);
        group.transformations.push_back(transformation);
      }

      int hmin, hmax, vmin, vmax;
      vp->coordinateSystem()->minMax(hmin, hmax, vmin, vmax);
      // If the coordinate system was extended in the negative direction (minimum decreased)
      // during this transformation AND the polyhedron is positioned at the lower extrema of
      // the coordiante system, update its position.  Fixes the problem with big extensions of
      // the CS if a polyhedron ends up outside coordinate system.
      const bool fixHorizontal = hmin < m_initCSHorizontalMin &&
                                 hmin >= (vp->localHorizontalMin() + m_horzOffset);
      const bool fixVertical   = vmin < m_initCSVerticalMin &&
                                 vmin >= (vp->localVerticalMin() + m_vertOffset);
      if (fixHorizontal && fixVertical) {
        vp->coordinateSystem()->setPolyhedronCoordinates(vp, hmin, vmin);
      } else if (fixHorizontal) {
        vp->coordinateSystem()->setPolyhedronCoordinates(vp, hmin, INT_MAX, false, true);
      } else if (fixVertical) {
        vp->coordinateSystem()->setPolyhedronCoordinates(vp, INT_MAX, vmin, true, false);
      }
      if (zeroDimensional) {
        vp->coordinateSystem()->setPolyhedronCoordinates(vp, vp->localHorizontalMin(), vp->localVerticalMin());
      } else if (oneDimensional) {
        vp->coordinateSystem()->setPolyhedronCoordinates(vp, INT_MAX, vp->localVerticalMin(), true, false);
      }
      CLINT_ASSERT(vp->scop() == polyhedron->scop(), "All statement occurrences in the transformation group must be in the same scop");
    }

    if (m_horzOffset != 0)
      emit movedHorizontally(m_horzOffset);
    if (m_vertOffset != 0)
      emit movedVertically(m_vertOffset);
  } else {
    for (VizPolyhedron *vp : selectedPolyhedra) {
      vp->coordinateSystem()->polyhedronUpdated(vp);
    }
  }

  if (!group.transformations.empty()) {
    polyhedron->scop()->transform(group);
    polyhedron->scop()->executeTransformationSequence();
    polyhedron->coordinateSystem()->projection()->updateInnerDependences();
    for (VizPolyhedron *vp : selectedPolyhedra) {
      vp->occurrenceChanged();
    }
  }
}


void VizManipulationManager::polyhedronAboutToDetach(VizPolyhedron *polyhedron) {
  CLINT_ASSERT(m_polyhedron == nullptr, "Active polyhedron is already being manipulated");
  m_polyhedron = polyhedron;
  ensureTargetConsistency();

  CLINT_ASSERT(m_polyhedron->coordinateSystem()->coordinateSystemRect().contains(polyhedron->pos()),
               "Coordinate system rectangle does not contain the polyhedron");
  m_detached = false;

  QPointF position = polyhedron->coordinateSystem()->mapToParent(polyhedron->pos());
  VizProjection::IsCsResult r = polyhedron->coordinateSystem()->projection()->isCoordinateSystem(position);
  CLINT_ASSERT(r.action() == VizProjection::IsCsAction::Found && r.coordinateSystem() == polyhedron->coordinateSystem(),
               "Polyhedron position is not found in the coordinate system it actually belongs to");
}

void VizManipulationManager::polyhedronDetaching(QPointF position) {
  QRectF mainRect = m_polyhedron->coordinateSystem()->coordinateSystemRect();
  if (!mainRect.contains(position)) {
    m_detached = true;
  }
}

void VizManipulationManager::rearrangePiles2D(std::vector<int> &createdBeta, bool pileDeleted, TransformationGroup &group, int pileIdx, size_t pileNb) {
  CLINT_ASSERT(createdBeta.size() >= 3, "Dimensionality mismatch");
  std::vector<int> pileBeta(std::begin(createdBeta), std::end(createdBeta) - 2);
  if (pileDeleted) {
    if (pileIdx == pileBeta.back() || pileIdx == pileBeta.back() + 1) {
      // Do nothing (cases DN1, DN2);
    } else {
      // Put before the given pile.
      group.transformations.push_back(Transformation::putBefore(pileBeta, pileIdx, pileNb));
    }
  } else {
    if (pileIdx == pileBeta.back()) {
      // Do nothing (case KN2);
    } else if (pileIdx > pileBeta.back()) {
      // Put before, account for pile not being deleted.
      group.transformations.push_back(Transformation::putBefore(pileBeta, pileIdx + 1, pileNb));
    } else {
      // Put before the given pile.
      group.transformations.push_back(Transformation::putBefore(pileBeta, pileIdx, pileNb));
    }
  }
}

void VizManipulationManager::rearrangeCSs2D(int coordinateSystemIdx, bool csDeleted, std::vector<int> &createdBeta, size_t pileSize, TransformationGroup &group) {
        // By analogy to insert pile.
  CLINT_ASSERT(createdBeta.size() >= 2, "Dimensionality mismatch");
  std::vector<int> csBeta(std::begin(createdBeta), std::end(createdBeta) - 1);
  if (csDeleted) {
    if (coordinateSystemIdx == csBeta.back() || coordinateSystemIdx == csBeta.back() + 1) {
      // Do nothing.
    } else {
      group.transformations.push_back(Transformation::putBefore(csBeta, coordinateSystemIdx, pileSize));
    }
  } else {
    if (coordinateSystemIdx == csBeta.back()) {
      // Do nothing.
    } else if (coordinateSystemIdx > csBeta.back()) {
  // FIXME: hack
  if (coordinateSystemIdx + 1 > pileSize)
    coordinateSystemIdx = pileSize - 1;
      group.transformations.push_back(Transformation::putBefore(csBeta, coordinateSystemIdx + 1, pileSize));
    } else {
      group.transformations.push_back(Transformation::putBefore(csBeta, coordinateSystemIdx, pileSize));
    }
  }
}

void VizManipulationManager::polyhedronHasDetached(VizPolyhedron *polyhedron) {
  CLINT_ASSERT(m_polyhedron == polyhedron, "Signaled end of polyhedron movement that was never initiated");
  m_polyhedron = nullptr;
  TransformationGroup group, iterGroup;

  const std::unordered_set<VizPolyhedron *> &selectedPolyhedra =
      polyhedron->coordinateSystem()->projection()->selectionManager()->selectedPolyhedra();
  CLINT_ASSERT(std::find(std::begin(selectedPolyhedra), std::end(selectedPolyhedra), polyhedron) != std::end(selectedPolyhedra),
               "The active polyhedra is not selected");
  if (m_detached) {
    QPointF position = polyhedron->coordinateSystem()->mapToParent(polyhedron->pos());
    VizProjection::IsCsResult r = polyhedron->coordinateSystem()->projection()->isCoordinateSystem(position);
    int dimensionality = -1;
    for (VizPolyhedron *vp : selectedPolyhedra) {
      dimensionality = std::max(dimensionality, vp->occurrence()->dimensionality());
    }
    VizCoordinateSystem *cs = polyhedron->coordinateSystem()->projection()->ensureCoordinateSystem(r, dimensionality);
    int hmin = INT_MAX, hmax = INT_MIN, vmin = INT_MAX, vmax = INT_MIN; // TODO: frequent functionality, should be extracted?
    // FIXME: all these assumes only all polyhedra belong to the same coordiante system.
    bool firstPolyhedron = true;
    for (VizPolyhedron *vp : selectedPolyhedra) {
      // Assume all selected polyhedra are moved to the same coordinate system.
      // Then only the first polyhedron may have an Insert action to create this system
      // while all other polyhedra will have the Found action.
      if (firstPolyhedron) {
        firstPolyhedron = false;
      } else {
        r.setFound(cs);
      }

      VizCoordinateSystem *oldCS = vp->coordinateSystem();
      size_t oldPileIdx, oldCsIdx;
      // FIXME: works for normalized scops only
      std::tie(oldPileIdx, oldCsIdx) = oldCS->projection()->csIndices(oldCS);
      bool oneDimensional = vp->occurrence()->dimensionality() < oldCS->verticalDimensionIdx();
      bool zeroDimensional = vp->occurrence()->dimensionality() < oldCS->horizontalDimensionIdx();

      bool movedToAnotherCS = oldCS != cs; // split is needed if the polygon was moved to a different CS

      if (!movedToAnotherCS) {
        vp->coordinateSystem()->resetPolyhedronPos(vp);
        continue;
      }


      bool csDeleted = false;
      bool pileDeleted = false;

      cs->reparentPolyhedron(vp);
      if (r.action() != VizProjection::IsCsAction::Found) {
        cs->resetPolyhedronPos(polyhedron);
      }
      // TODO: otherwise, shift it to the position it ended up graphically (reparent does it visually, but we must create a shift Transformation)
      hmin = std::min(hmin, vp->localHorizontalMin());
      hmax = std::max(hmax, vp->localHorizontalMax());
      vmin = std::min(vmin, vp->localVerticalMin());
      vmax = std::max(vmax, vp->localVerticalMax());
      if (oldCS->isEmpty()) {
        csDeleted = true;
        if (oldCS->projection()->pileCSNumber(oldPileIdx) == 1) {
          pileDeleted = true;
        }

        oldCS->projection()->deleteCoordinateSystem(oldCS);
      }

      CLINT_ASSERT(oneDimensional ? csDeleted : true, "In 1D cases, CS should be deleted");
      CLINT_ASSERT(zeroDimensional ? pileDeleted : true, "In 0D cases, pile should be deleted");

      size_t pileNb = cs->projection()->pileNumber();

      // Constructing transformation group.
      const std::vector<int> &beta = vp->occurrence()->betaVector();
      std::vector<int> createdBeta(beta);
      bool splitHorizontal = false, splitVertical = false;

      bool actionWithinPile = (r.action() == VizProjection::IsCsAction::Found ||
                               r.action() == VizProjection::IsCsAction::InsertCS) && r.pileIdx() == oldPileIdx;
      // Splitting the occurrence out.
      if (!csDeleted) { // if coordinate system was deleted, no inner split needed
        size_t csSize = oldCS->countPolyhedra() + 1; // it was already removed from CS
        std::vector<int> splitBeta(beta);
        splitBeta.push_back(424242);
        qDebug() << "inner split" << csSize << QVector<int>::fromStdVector(splitBeta);
        rearrangeCSs2D(csSize, true, splitBeta, csSize, iterGroup);
        CLINT_ASSERT(csSize >= 2, "Split requested where it should not happen");
        splitBeta.erase(std::end(splitBeta) - 1);
        splitBeta.back() = csSize - 2;
        iterGroup.transformations.push_back(Transformation::splitAfter(splitBeta));

        splitVertical = true;
      }
      if (!pileDeleted && !actionWithinPile) { // if pile was deleted or fusion is done within a pile, no outer split needed
        size_t pileSize = cs->projection()->pileCSNumber(oldPileIdx) + (csDeleted || !actionWithinPile ? 1 : 0); // if a cs was deleted, still account for it.
        std::vector<int> splitBeta(beta);
        // If inner split took place, the beta is changed.
        if (splitVertical) {
          splitBeta[splitBeta.size() - 2]++;
          splitBeta[splitBeta.size() - 1] = 0;
        }
        if (oneDimensional) {
          splitBeta.push_back(424242);
        }
        qDebug() << "outer split" << pileSize << QVector<int>::fromStdVector(splitBeta);
        rearrangeCSs2D(pileSize, true, splitBeta, pileSize, iterGroup);
        CLINT_ASSERT(pileSize >= 2, "Split requested where it should not happen");
        CLINT_ASSERT(splitBeta.size() >= 2, "Dimensionality mismatch");
        splitBeta.erase(std::end(splitBeta) - 1);
        splitBeta.back() = pileSize - 2;
        iterGroup.transformations.push_back(Transformation::splitAfter(splitBeta));

        splitHorizontal = true;
      }

      if (vp->coordinateSystem()->isVerticalAxisVisible() && vp->coordinateSystem()->isHorizontalAxisVisible()) {
        CLINT_ASSERT(createdBeta.size() >= 3, "Dimensionality mismatch");
        if (splitHorizontal) {
          qDebug() << "sh";
          createdBeta[createdBeta.size() - 1] = 0;
          createdBeta[createdBeta.size() - 2] = 0;
          createdBeta[createdBeta.size() - 3] += 1;
        } else if (splitVertical) {
          qDebug() << "sv";
          createdBeta[createdBeta.size() - 1] = 0;
          createdBeta[createdBeta.size() - 2] += 1;
        }
      } else if (vp->coordinateSystem()->isHorizontalAxisVisible()) {
        CLINT_ASSERT(createdBeta.size() >= 2, "Dimensionality mismatch");
        if (splitHorizontal) {
          createdBeta[createdBeta.size() - 1] = 0;
          createdBeta[createdBeta.size() - 2] += 1;
        }
      } else if (!vp->coordinateSystem()->isVerticalAxisVisible() && !vp->coordinateSystem()->isHorizontalAxisVisible()) {
        // Do nothing
      } else {
        CLINT_UNREACHABLE;
      }

      qDebug() << "beta update successful" << QVector<int>::fromStdVector(createdBeta);

      size_t targetPileIdx, targetCSIdx;
      std::tie(targetPileIdx, targetCSIdx) = cs->projection()->csIndices(cs);

      qDebug() << "targeting pile" << targetPileIdx << "cs" << targetCSIdx;

      if (r.action() == VizProjection::IsCsAction::Found) {
        // Fusion required.
        std::vector<int> fuseBeta = cs->betaPrefix(); // if found, it contains at least one polygon (other than vp) with correct beta.
        qDebug() << QVector<int>::fromStdVector(fuseBeta) << r.coordinateSystemIdx() << r.pileIdx();
        CLINT_ASSERT(fuseBeta[fuseBeta.size() - 1] == r.coordinateSystemIdx(), "Beta consistency violated");
        CLINT_ASSERT(fuseBeta[fuseBeta.size() - 2] == r.pileIdx(), "Beta consistency violated");

        size_t pileIdx = r.pileIdx();
        if (pileDeleted && oldPileIdx < pileIdx) {
          pileIdx--;
          fuseBeta[fuseBeta.size() - 2]--;
        }

        // TODO: same for CS?
        size_t csIdx = r.coordinateSystemIdx();
        if (csDeleted && actionWithinPile && oldCsIdx < csIdx) {
          csIdx--;
          fuseBeta[fuseBeta.size() - 1]--;
        }

        // First, rearrange and fuse on the outer level.  For fusion's sake, put the statement after the pile to fuse with.
//        rearrangePiles2D(createdBeta, true, group, pileIdx + 1 + (pileDeleted ? 0 : 1), pileNb + (pileDeleted ? 0 : 1));
        // Fuse if not within pile transformation.
        if (!actionWithinPile) {
          if (oneDimensional)
            createdBeta.push_back(424242);
          qDebug() << "outer fuse" << pileNb + (actionWithinPile ? 0 : 1) << pileIdx << QVector<int>::fromStdVector(fuseBeta) << QVector<int>::fromStdVector(createdBeta);
          rearrangePiles2D(createdBeta, false, iterGroup, pileIdx + 1, pileNb + (actionWithinPile ? 0 : 1));
          std::vector<int> outerFuseBeta(std::begin(fuseBeta), std::end(fuseBeta) - 1);
          iterGroup.transformations.push_back(Transformation::fuseNext(outerFuseBeta));
        }

        // Finally, rearrange and fuse on the inner level.
        size_t pileSize = cs->projection()->pileCSNumber(pileIdx) + (csDeleted && actionWithinPile ? 0 : 1); // extra CS created and is not visually present
        if (!actionWithinPile) { // If outer fuse took place, this occurence is the last in pile
          createdBeta[createdBeta.size() - 3] = pileIdx;
          createdBeta[createdBeta.size() - 2] = pileSize - 1;
          createdBeta[createdBeta.size() - 1] = 0;
        }
        qDebug() << "inner fuse" << pileSize << csIdx << QVector<int>::fromStdVector(createdBeta);
        if (!oneDimensional) {
          rearrangeCSs2D(csIdx + 1, false, createdBeta, pileSize + csDeleted, iterGroup);
          iterGroup.transformations.push_back(Transformation::fuseNext(fuseBeta));
        }

      } else if (r.action() == VizProjection::IsCsAction::InsertPile) {
        if (oneDimensional)
          createdBeta.push_back(424242);
        if (zeroDimensional) {
          createdBeta.push_back(424242);
          createdBeta.push_back(424242);
        }
        qDebug() << "insert pile";
        rearrangePiles2D(createdBeta, pileDeleted, iterGroup, r.pileIdx(), pileNb);
      } else if (r.action() == VizProjection::IsCsAction::InsertCS) {
//        if (oldPileIdx == r.pileIdx()) { // same pile -- reorder only
        if (actionWithinPile) {
          size_t pileSize = cs->projection()->pileCSNumber(r.pileIdx());
          if (oneDimensional)
            createdBeta.push_back(424242);
          qDebug() << "insert cs" << pileSize << QVector<int>::fromStdVector(createdBeta);
          rearrangeCSs2D(r.coordinateSystemIdx(), csDeleted, createdBeta, pileSize, iterGroup);
        } else { // different piles -- fusion required
          qDebug() << "insert cs" << QVector<int>::fromStdVector(createdBeta);

          size_t pileIdx = r.pileIdx();
          if (pileDeleted && oldPileIdx < pileIdx) {
            pileIdx--;
          }

          // TODO: same for CS?
          size_t csIdx = r.coordinateSystemIdx();
          if (csDeleted && actionWithinPile && oldCsIdx < csIdx) {
            csIdx--;
          }
          std::vector<int> fuseBeta { (int) pileIdx, (int) csIdx }; // <0,1> projection; otherwise first part of the beta-prefix ("projection" prefix) needed

          // First, rearrange and fuse on the outer level.  For fusion's sake, put the statement after the pile to fuse with.
//          rearrangePiles2D(createdBeta, true, group, pileIdx + 1 + (pileDeleted ? 0 : 1), pileNb + (pileDeleted ? 0 : 1));
          if (!actionWithinPile) {
            if (oneDimensional)
              createdBeta.push_back(424242);
            rearrangePiles2D(createdBeta, false, iterGroup, pileIdx + 1, pileNb + (actionWithinPile ? 0 : 1));
            qDebug() << "outer fuse" << pileNb + (pileDeleted ? 0 : 1) << pileIdx << QVector<int>::fromStdVector(fuseBeta);
            std::vector<int> outerFuseBeta(std::begin(fuseBeta), std::end(fuseBeta) - 1);
            iterGroup.transformations.push_back(Transformation::fuseNext(outerFuseBeta));
          }

          // Finally, rearrange and fuse on the inner level.
          size_t pileSize = cs->projection()->pileCSNumber(pileIdx); // extra CS created is actually present
          if (!actionWithinPile) { // If outer fuse took place, this occurence is the last in pile
            createdBeta[createdBeta.size() - 3] = pileIdx;
            createdBeta[createdBeta.size() - 2] = pileSize - 1;
            createdBeta[createdBeta.size() - 1] = 0;
          }
          qDebug() << "inner fuse" << pileSize << csIdx << QVector<int>::fromStdVector(createdBeta) << oneDimensional;
          if (!oneDimensional)
            rearrangeCSs2D(csIdx, csDeleted, createdBeta, pileSize, iterGroup); // no (csIdx + 1) since to fusion required; putting before.

        }
      } else {
        CLINT_UNREACHABLE;
      }

      Transformer *transformer = new ClayScriptGenerator(std::cerr);
      transformer->apply(nullptr, iterGroup);
      delete transformer;

      // Update betas after each polyhedron moved as we use them to determine beta-prefixes
      // of the coordinate systems.
      if (!iterGroup.transformations.empty()) {
        polyhedron->scop()->transform(iterGroup);
      }
//      polyhedron->scop()->remapBetas(iterGroup);

      std::copy(std::begin(iterGroup.transformations), std::end(iterGroup.transformations), std::back_inserter(group.transformations));
      iterGroup.transformations.clear();
    }

    // TODO: provide functionality for both simultaneously with a single repaint (setMinMax?)
    cs->projection()->ensureFitsHorizontally(cs, hmin, hmax);
    cs->projection()->ensureFitsVertically(cs, vmin, vmax);
  } else {
    for (VizPolyhedron *vp : selectedPolyhedra) {
      vp->coordinateSystem()->resetPolyhedronPos(vp);
    }
  }

  if (!group.transformations.empty()) {
//    polyhedron->scop()->transform(group);
    polyhedron->scop()->executeTransformationSequence();
    polyhedron->coordinateSystem()->projection()->updateOuterDependences();
    polyhedron->coordinateSystem()->projection()->updateInnerDependences();
  }
}

void VizManipulationManager::pointAboutToMove(VizPoint *point) {
  VizSelectionManager *selectionManager =
      point->coordinateSystem()->projection()->selectionManager();
  const std::unordered_set<VizPoint *> selectedPoints =
      selectionManager->selectedPoints();

  m_pointDetachState = PT_NODETACH;
  if (selectedPoints.size() == 0)
    return;

  bool samePolyhedron = true;
  int selectionHorizontalMin = INT_MAX,
      selectionHorizontalMax = INT_MIN,
      selectionVerticalMin   = INT_MAX,
      selectionVerticalMax   = INT_MIN;
  for (VizPoint *pt : selectedPoints) {
    int horzCoordinate, vertCoordinate;
    std::tie(horzCoordinate, vertCoordinate) = pt->scatteredCoordinates();

    selectionHorizontalMin = std::min(selectionHorizontalMin, horzCoordinate);
    selectionHorizontalMax = std::max(selectionHorizontalMax, horzCoordinate);
    selectionVerticalMin   = std::min(selectionVerticalMin, vertCoordinate);
    selectionVerticalMax   = std::max(selectionVerticalMax, vertCoordinate);

    samePolyhedron = samePolyhedron && (pt->polyhedron() == point->polyhedron());
  }

  if (!samePolyhedron)
    return;

  VizPolyhedron *polyhedron = point->polyhedron();
  std::unordered_set<VizPoint *> allPoints = polyhedron->points();

  int polyhedronHorizontalSize = polyhedron->localHorizontalMax() -
      polyhedron->localHorizontalMin();
  int polyhedronVerticalSize = polyhedron->localVerticalMax() -
      polyhedron->localVerticalMin();

  // Condition 1: adjacent to one of borders and consecutive (w/o localdim)
  // std::map is ordered by key
  // Condition 2: there does not exist a point such that it has a
  // horizontalCoorinate less than or equal to horizontalMax and it is not in
  // the selection (for the first case; others adapted accordingly).
  if (selectionHorizontalMin == polyhedron->localHorizontalMin() && // Attached to the horizontal beginning
      selectionHorizontalMax - selectionHorizontalMin != polyhedronHorizontalSize) {   // Not all lines selected
    auto missingPoint = std::find_if(std::begin(allPoints), std::end(allPoints), [selectionHorizontalMax,&selectedPoints](VizPoint *vp) {
      int horzCoordinate, vertCoordinate;
      std::tie(horzCoordinate, vertCoordinate) = vp->scatteredCoordinates();
      return horzCoordinate <= selectionHorizontalMax && selectedPoints.count(vp) == 0;
    });

    if (missingPoint == std::end(allPoints)) {
      m_pointDetachState = PT_DETACH_VERTICAL;
      m_pointDetachValue = polyhedron->localHorizontalMin() + (selectionHorizontalMax - selectionHorizontalMin + 1);
      m_pointDetachLast = false;
    }
  } else if (selectionHorizontalMax == polyhedron->localHorizontalMax() &&
             selectionHorizontalMax - selectionHorizontalMin != polyhedronHorizontalSize) {
    auto missingPoint = std::find_if(std::begin(allPoints), std::end(allPoints), [selectionHorizontalMin,&selectedPoints](VizPoint *vp) {
      int horzCoordinate, vertCoordinate;
      std::tie(horzCoordinate, vertCoordinate) = vp->scatteredCoordinates();
      return horzCoordinate >= selectionHorizontalMin && selectedPoints.count(vp) == 0;
    });
    if (missingPoint == std::end(allPoints)) {
      m_pointDetachState = PT_DETACH_VERTICAL;
      m_pointDetachValue = polyhedron->localHorizontalMax() - (selectionHorizontalMax - selectionHorizontalMin + 1);
      m_pointDetachLast = true;
    }

  } else if (selectionVerticalMin == polyhedron->localVerticalMin() &&
             selectionVerticalMax - selectionVerticalMin != polyhedronVerticalSize) {
    auto missingPoint = std::find_if(std::begin(allPoints), std::end(allPoints), [selectionVerticalMax,&selectedPoints](VizPoint *vp) {
      int horzCoordinate, vertCoordinate;
      std::tie(horzCoordinate, vertCoordinate) = vp->scatteredCoordinates();
      return vertCoordinate <= selectionVerticalMax && selectedPoints.count(vp) == 0;
    });

    if (missingPoint == std::end(allPoints)) {
      m_pointDetachState = PT_DETACH_HORIZONTAL;
      m_pointDetachValue = polyhedron->localVerticalMin() + (selectionVerticalMax - selectionVerticalMin + 1);
      m_pointDetachLast = false;
    }
  } else if (selectionVerticalMax == polyhedron->localVerticalMax() &&
             selectionVerticalMax - selectionVerticalMin != polyhedronVerticalSize) {
    auto missingPoint = std::find_if(std::begin(allPoints), std::end(allPoints), [selectionVerticalMin,&selectedPoints](VizPoint *vp) {
      int horzCoordinate, vertCoordinate;
      std::tie(horzCoordinate, vertCoordinate) = vp->scatteredCoordinates();
      return vertCoordinate >= selectionVerticalMin && selectedPoints.count(vp) == 0;
    });

    if (missingPoint == std::end(allPoints)) {
      m_pointDetachState = PT_DETACH_HORIZONTAL;
      m_pointDetachValue = polyhedron->localVerticalMax() - (selectionVerticalMax - selectionVerticalMin + 1);
      m_pointDetachLast = true;
    }
  } else if (selectedPoints.size() == allPoints.size() &&
             polyhedron->coordinateSystem()->countPolyhedra() == 2) { // TODO: allow collapsing a single statement
                                                                      // this requires figuring out what is the statement it can collapse
                                                                      // with, extracting both to a separate loop, collapsing the loop
                                                                      // end fusing back; chlore has functionality to extract statements
                                                                      // until given depth
    Transformation collapse = Transformation::collapse(polyhedron->coordinateSystem()->betaPrefix());
    if (polyhedron->occurrence()->scop()->guessInverseTransformation(collapse)) { // we could try to figure out shifts/reshapes to enable it
                                                                                  // if it is not with whiteboxing or mapping deviation
      m_pointDetachState = PT_ATTACH;
    } else {
      m_pointDetachState = PT_NODETACH;
    }
  } else {
    m_pointDetachState = PT_NODETACH;
    return; // no transformation;
  }
}

void VizManipulationManager::pointHasMoved(VizPoint *point) {
  switch (m_pointDetachState) {
  case PT_NODETACH:
  case PT_DETACH_VERTICAL:
  case PT_DETACH_HORIZONTAL:
  case PT_ATTACH:
  {
    std::set<VizPolyhedron *> polyhedra;
    for (VizPoint *vp : point->coordinateSystem()->projection()->selectionManager()->selectedPoints())
      polyhedra.insert(vp->polyhedron());
    for (VizPolyhedron *polyhedron : polyhedra)
      polyhedron->resetPointPositions();
    return;
  }
    break;
  case PT_DETACHED_HORIZONTAL:
  case PT_DETACHED_VERTICAL:
  {
    VizPolyhedron *polyhedron = new VizPolyhedron(nullptr, point->coordinateSystem());
    VizSelectionManager *sm = point->coordinateSystem()->projection()->selectionManager();
    const std::unordered_set<VizPoint *> selectedPoints = sm->selectedPoints();
    CLINT_ASSERT(std::find(std::begin(selectedPoints), std::end(selectedPoints), point) != std::end(selectedPoints),
                 "The point being manipulated is not selected");

    VizPolyhedron *oldPolyhedron = point->polyhedron();

    int dimensionIdx;
    if (m_pointDetachState == PT_DETACHED_HORIZONTAL) {
      dimensionIdx = oldPolyhedron->coordinateSystem()->verticalDimensionIdx();
    } else {
      dimensionIdx = oldPolyhedron->coordinateSystem()->horizontalDimensionIdx();
    }
    int depth = oldPolyhedron->occurrence()->depth(dimensionIdx);

    for (VizPoint *vp : selectedPoints) {
      polyhedron->reparentPoint(vp);
    }
    oldPolyhedron->updateShape();
    polyhedron->setColor(Qt::white);
    polyhedron->updateShape();
    point->coordinateSystem()->insertPolyhedronAfter(polyhedron, oldPolyhedron);

    std::vector<int> betaLoop(oldPolyhedron->occurrence()->betaVector());
    betaLoop.erase(betaLoop.end() - 1);
    TransformationGroup group;
    std::vector<int> issVector = oldPolyhedron->occurrence()->findBoundlikeForm(
          m_pointDetachLast ? ClintStmtOccurrence::Bound::UPPER :
                              ClintStmtOccurrence::Bound::LOWER,
          depth - 1,
          m_pointDetachValue);
    CLINT_ASSERT(issVector.size() >= 2, "malformed ISS vector");

    int nbInputDims = oldPolyhedron->occurrence()->untiledBetaVector().size() - 1;

    // Exclude the statement from its position, iss works only with loops.
    int lastValue = oldPolyhedron->scop()->lastValueInLoop(betaLoop);
    int nbStatements = oldPolyhedron->scop()->occurrences(betaLoop).size();
    CLINT_ASSERT(nbStatements - 1 == lastValue, "Beta map is not normalized");
    std::vector<int> beta = oldPolyhedron->occurrence()->betaVector();
    int originalPosition = beta.back();
    TransformationGroup preGroup, postGroup;
    if (nbStatements != 1) {
      if (originalPosition != nbStatements - 1) { // if it is not already the last statement
        preGroup.transformations.push_back(Transformation::putAfterLast(beta, nbStatements));
      }
      std::vector<int> splitBeta(betaLoop);
      splitBeta.push_back(lastValue - 1);
      preGroup.transformations.push_back(Transformation::splitAfter(splitBeta));
      betaLoop.back() += 1; // splitted
    }

    group.transformations.push_back(Transformation::issFromConstraint(betaLoop, issVector, nbInputDims));

    // Put statement back to its original position along with the created statement.
    if (nbStatements != 1) {
      betaLoop.back() -= 1; // previous loop before splitted
      postGroup.transformations.push_back(Transformation::fuseNext(betaLoop));
      std::vector<int> betaStmt(betaLoop);
      if (originalPosition != nbStatements - 1) {
        betaStmt.push_back(nbStatements - 1);
        postGroup.transformations.push_back(Transformation::putBefore(betaStmt, originalPosition, nbStatements + 1));
        postGroup.transformations.push_back(Transformation::putAfter(betaStmt, originalPosition, nbStatements + 1));
      }
    }

    // Execute statement split and index-set-split.
    // Beta-remap needed after split, and executeTransformSequence will create the new statement.
    if (preGroup.transformations.size() != 0) {
      oldPolyhedron->scop()->transform(preGroup);
    }
    oldPolyhedron->scop()->transform(group);
    oldPolyhedron->scop()->executeTransformationSequence();

    // Execute statement fuse.  The new statement already exists, so we can safely remap it after fuse.
    if (postGroup.transformations.size() != 0) {
      oldPolyhedron->scop()->transform(postGroup);
    }
    oldPolyhedron->scop()->executeTransformationSequence();

    originalPosition += 1;

    betaLoop.push_back(originalPosition);
    ClintStmtOccurrence *occurrence = oldPolyhedron->scop()->occurrence(betaLoop);
    polyhedron->setOccurrenceSilent(occurrence);
    polyhedron->occurrenceChanged(); // Occurrence is not set when the transformation is executed so we have to notify changes manually.
    oldPolyhedron->updateInternalDependences();
    polyhedron->updateInternalDependences();
    polyhedron->coordinateSystem()->projection()->updateInnerDependences();
  }
    break;

  case PT_ATTACHED:
  {
    // XXX: makes assumptions about beta-structure and values (fine for now, given that we can recover betas with chlore)
    VizPolyhedron *currentPolyhedron = point->polyhedron();
    VizPolyhedron *remaining = nullptr;
    std::vector<int> loopBeta = currentPolyhedron->coordinateSystem()->betaPrefix();
    std::vector<int> remainingBeta(loopBeta), removingBeta(loopBeta);
    std::vector<int> currentBeta = currentPolyhedron->occurrence()->betaVector();
    remainingBeta.push_back(0);
    removingBeta.push_back(1);
    if (BetaUtility::isPrefixOrEqual(removingBeta, currentBeta)) {
      // Reparent all selection points to another polyhedron.
      VizPolyhedron *otherPolyhedron = currentPolyhedron->coordinateSystem()->polyhedron(remainingBeta);
      CLINT_ASSERT(otherPolyhedron != 0, "Couldn't find a matching polyhedron for collapse");
      VizSelectionManager *sm = currentPolyhedron->coordinateSystem()->projection()->selectionManager();
      const std::unordered_set<VizPoint *> selectedPoints = sm->selectedPoints();
      for (VizPoint *point : selectedPoints) {
        otherPolyhedron->reparentPoint(point);
      }
      currentPolyhedron->coordinateSystem()->removePolyhedron(currentPolyhedron);
      delete currentPolyhedron;
      remaining = otherPolyhedron;
    } else if (BetaUtility::isPrefixOrEqual(remainingBeta, currentBeta)) {
      // Reparent all points of another polyhedron to the current.
      VizPolyhedron *otherPolyhedron = currentPolyhedron->coordinateSystem()->polyhedron(removingBeta);
      CLINT_ASSERT(otherPolyhedron != 0, "Couldn't find a matching polyhedron for collapse");
      std::unordered_set<VizPoint *> points = otherPolyhedron->points();
      for (VizPoint *point : points) {
        currentPolyhedron->reparentPoint(point);
      }
      currentPolyhedron->coordinateSystem()->removePolyhedron(otherPolyhedron);
      delete otherPolyhedron;
      remaining = currentPolyhedron;
    } else {
      CLINT_UNREACHABLE;
    }

    remaining->updateShape();

    TransformationGroup group;
    group.transformations.push_back(Transformation::collapse(loopBeta));
    remaining->scop()->transform(group);
    remaining->scop()->executeTransformationSequence();
    remaining->updateInternalDependences();
    remaining->coordinateSystem()->projection()->updateInnerDependences();
  }
    break;
  }
}

void VizManipulationManager::pointMoving(QPointF position) {
  switch (m_pointDetachState) {
  case PT_NODETACH:
    return;
    break;
  case PT_DETACH_HORIZONTAL:
  case PT_DETACHED_HORIZONTAL:
    if (fabs(position.y()) >= 4.0) // FIXME: hardcode
      m_pointDetachState = PT_DETACHED_HORIZONTAL;
    else
      m_pointDetachState = PT_DETACH_HORIZONTAL;
    break;
  case PT_DETACH_VERTICAL:
  case PT_DETACHED_VERTICAL:
    if (fabs(position.x()) >= 4.0)
      m_pointDetachState = PT_DETACHED_VERTICAL;
    else
      m_pointDetachState = PT_DETACH_VERTICAL;
    break;
  case PT_ATTACH:
  case PT_ATTACHED:
    if (fabs(position.x()) >= 4.0)
      m_pointDetachState = PT_ATTACHED;
    else
      m_pointDetachState = PT_ATTACH;
  }
}

void VizManipulationManager::polyhedronAboutToResize(VizPolyhedron *polyhedron, Dir direction) {
  CLINT_ASSERT(polyhedron != nullptr, "cannot resize null polyhedron");
  m_polyhedron = polyhedron;
  m_direction = direction;

  // Do not allow multiple polyhedra resize yet.  This would require creating a graphicsGroup
  // and adding handles for the group to ensure UI consistency.
  std::unordered_set<VizPolyhedron *> selectedPolyhedra =
      polyhedron->coordinateSystem()->projection()->selectionManager()->selectedPolyhedra();
  CLINT_ASSERT(selectedPolyhedra.size() == 0, "No selection allowed when resizing");

  // Do not allow grain on the invisible dimension.
  bool oneDimensional = polyhedron->occurrence()->dimensionality() < polyhedron->coordinateSystem()->verticalDimensionIdx();
  bool zeroDimensional = polyhedron->occurrence()->dimensionality() < polyhedron->coordinateSystem()->horizontalDimensionIdx();
  bool aloneInLoop = polyhedron->coordinateSystem()->countPolyhedra() == 1;
  bool oneLine = polyhedron->localVerticalMin() == polyhedron->localVerticalMax();
  bool oneColumn = polyhedron->localHorizontalMin() == polyhedron->localHorizontalMax();

  const std::vector<int> &beta = polyhedron->occurrence()->betaVector();
  bool embedded = false;
  if (!zeroDimensional && oneDimensional &&
      (direction == Dir::UP || direction == Dir::DOWN)) {
    embedded = false;
  } else if (zeroDimensional) {
    embedded = false;
  } else if (!oneDimensional && !zeroDimensional &&
      beta.size() != polyhedron->occurrence()->depth(polyhedron->coordinateSystem()->verticalDimensionIdx()) + 1) {
    embedded = false;
  } else if (!oneLine && (direction == Dir::UP || direction == Dir::DOWN)) {
    embedded = false;
  } else if (!oneColumn && (direction == Dir::LEFT || direction == Dir::RIGHT)) {
    embedded = false;
  } else {
    Transformation embedTransformation = Transformation::unembed(beta);
    embedded = static_cast<bool>(polyhedron->occurrence()->scop()->guessInverseTransformation(embedTransformation));
  }

  if (zeroDimensional && (direction == Dir::LEFT || direction == Dir::RIGHT) && aloneInLoop) {
    m_creatingDimension = -1;
    m_resizing = false;
  } else if (!zeroDimensional && oneDimensional && (direction == Dir::UP || direction == Dir::DOWN) && aloneInLoop) {
    m_creatingDimension = -2;
    m_resizing = false;
  } else if (embedded && aloneInLoop && (direction == Dir::LEFT || direction == Dir::RIGHT) && oneDimensional) {
    m_creatingDimension = -3;
    m_resizing = false;
  } else if (embedded && aloneInLoop && (direction == Dir::UP || direction == Dir::DOWN)) {
    m_creatingDimension = -4;
    m_resizing = false;
  } else if ((!zeroDimensional && !oneDimensional) ||
             (oneDimensional && (direction == Dir::LEFT || direction == Dir::RIGHT))) {
    if (((direction == Dir::LEFT || direction == Dir::RIGHT) && oneColumn) ||
        ((direction == Dir::UP || direction == Dir::DOWN) && oneLine)) {
      m_creatingDimension = 0;
      m_resizing = false;
      polyhedron->resetPointPositions();
      return;
    } else {
      m_creatingDimension = 0;
      m_resizing = true;
    }
  } else {
    m_creatingDimension = 0;
    m_resizing = false;
    polyhedron->resetPointPositions();
    return;
  }
}

void VizManipulationManager::polyhedronHasCreatedDimension(VizPolyhedron *polyhedron) {
  if (m_creatingDimension <= 0) {
    m_creatingDimension = 0;
    return;
  }
  bool horizontal = (m_creatingDimension % 2) == 1;
  bool unembed = m_creatingDimension > 2;
  m_creatingDimension = 0;

  TransformationGroup group;
  if (unembed) {
    group.transformations.push_back(Transformation::unembed(polyhedron->occurrence()->betaVector()));
  } else {
    group.transformations.push_back(Transformation::embed(polyhedron->occurrence()->betaVector()));
  }

  if (!group.transformations.empty()) {
    polyhedron->scop()->transform(group);
    if (!unembed) {
      polyhedron->scop()->executeTransformationSequence();
    }
    if (horizontal) {
      polyhedron->coordinateSystem()->setHorizontalDimensionIdx(
            unembed ? VizProperties::NO_DIMENSION :
            polyhedron->coordinateSystem()->projection()->horizontalDimensionIdx());
    } else {
      polyhedron->coordinateSystem()->setVerticalDimensionIdx(
            unembed ? VizProperties::NO_DIMENSION :
            polyhedron->coordinateSystem()->projection()->verticalDimensionIdx());
    }
    if (unembed) {
      polyhedron->scop()->executeTransformationSequence();
    }
    polyhedron->coordinateSystem()->update();
  }
  m_polyhedron = nullptr;
}

void VizManipulationManager::polyhedronHasResized(VizPolyhedron *polyhedron) {
  CLINT_ASSERT(m_polyhedron == polyhedron, "Wrong polyhedron finished resizing");

  if (m_creatingDimension) {
    polyhedronHasCreatedDimension(polyhedron);
  }

  if (!m_resizing) {
    return;
  }
  m_resizing = false;

  bool isHorizontal;
  if (m_direction == Dir::LEFT || m_direction == Dir::RIGHT) {
    isHorizontal = true;
  } else if (m_direction == Dir::UP || m_direction == Dir::DOWN) {
    isHorizontal = false;
  } else {
    CLINT_UNREACHABLE;
  }

  // Examine if grain was already done and that regrain is possible.
  size_t horizontalDimIdx = polyhedron->coordinateSystem()->horizontalDimensionIdx();
  size_t verticalDimIdx   = polyhedron->coordinateSystem()->verticalDimensionIdx();
  size_t horizontalDepth = horizontalDimIdx != VizProperties::NO_DIMENSION ?
        polyhedron->occurrence()->depth(horizontalDimIdx) :
        VizProperties::NO_DIMENSION;
  size_t verticalDepth   = verticalDimIdx != VizProperties::NO_DIMENSION ?
        polyhedron->occurrence()->depth(verticalDimIdx) :
        VizProperties::NO_DIMENSION;
  size_t depth;
  if (m_direction == Dir::LEFT || m_direction == Dir::RIGHT) {
    depth = horizontalDepth;
  } else {
    depth = verticalDepth;
  }
  Transformation densify = Transformation::densify(m_polyhedron->occurrence()->betaVector(),
                                                   depth);
  boost::optional<Transformation> inverse = m_polyhedron->occurrence()->scop()->guessInverseTransformation(densify);
  int preGrainAmount = inverse ? inverse.value().constantAmount() : 1;

  int horizontalRange = (m_polyhedron->localHorizontalMax() - m_polyhedron->localHorizontalMin());
  int verticalRange   = (m_polyhedron->localVerticalMax() - m_polyhedron->localVerticalMin());

  int grainAmount;
  switch (m_direction) {
  case Dir::LEFT:
    grainAmount = -round(static_cast<double>(m_horzOffset) * preGrainAmount / horizontalRange) + preGrainAmount;
    break;
  case Dir::RIGHT:
    grainAmount = round(static_cast<double>(m_horzOffset) * preGrainAmount / horizontalRange) + preGrainAmount;
    break;
  case Dir::UP:
    grainAmount = -round(static_cast<double>(m_vertOffset) * preGrainAmount / verticalRange) + preGrainAmount;
    break;
  case Dir::DOWN:
    grainAmount = round(static_cast<double>(m_vertOffset) * preGrainAmount / verticalRange) + preGrainAmount;
    break;
  }

  TransformationGroup group;
  // Check if reverse needed
  bool reverse = false;
  if (grainAmount < 0) {
    reverse = true;
    int dimension, halfShiftAmount;
    if (m_direction == Dir::LEFT || m_direction == Dir::RIGHT) {
      dimension = m_polyhedron->coordinateSystem()->horizontalDimensionIdx() + 1;
      halfShiftAmount = m_direction == Dir::RIGHT ?
            m_polyhedron->localHorizontalMin() :
            m_polyhedron->localHorizontalMax();
    } else {
      dimension = m_polyhedron->coordinateSystem()->verticalDimensionIdx() + 1;
      halfShiftAmount = m_direction == Dir::UP ?
            m_polyhedron->localVerticalMin() :
            m_polyhedron->localVerticalMax();
    }
    group.transformations.push_back(Transformation::reverse(
                                      polyhedron->occurrence()->betaVector(),
                                      dimension));
    if (halfShiftAmount != 0) {
      group.transformations.push_back(Transformation::constantShift(
                                        polyhedron->occurrence()->betaVector(),
                                        dimension, -2 * halfShiftAmount));
    }

    grainAmount = -grainAmount;
  }

  if (preGrainAmount != 1 && preGrainAmount != grainAmount) {
    group.transformations.push_back(densify);
  }

  if (grainAmount > 1 && preGrainAmount != grainAmount) {
    if (m_direction == Dir::LEFT || m_direction == Dir::RIGHT) {
      group.transformations.push_back(Transformation::grain(
                                        polyhedron->occurrence()->betaVector(),
                                        horizontalDepth,
                                        grainAmount));

      if (m_polyhedron->localHorizontalMin() != 0 && m_direction == Dir::RIGHT) {
        group.transformations.push_back(Transformation::constantShift(
                                          polyhedron->occurrence()->betaVector(),
                                          horizontalDepth,
                                          m_polyhedron->localHorizontalMin() * (grainAmount - 1)));
      } else if (m_polyhedron->localHorizontalMax() != 0 && m_direction == Dir::LEFT) {
        group.transformations.push_back(Transformation::constantShift(
                                          polyhedron->occurrence()->betaVector(),
                                          horizontalDepth,
                                          m_polyhedron->localHorizontalMax() * (grainAmount - 1)));
      }
    } else if (m_direction == Dir::DOWN || m_direction == Dir::UP) {
      group.transformations.push_back(Transformation::grain(
                                        polyhedron->occurrence()->betaVector(),
                                        verticalDepth,
                                        grainAmount));

      if (m_polyhedron->localVerticalMin() != 0 && m_direction == Dir::UP) {
        group.transformations.push_back(Transformation::constantShift(
                                          polyhedron->occurrence()->betaVector(),
                                          verticalDepth,
                                          m_polyhedron->localVerticalMin() * (grainAmount - 1)));
      } else if (m_polyhedron->localVerticalMax() != 0 && m_direction == Dir::DOWN) {
        group.transformations.push_back(Transformation::constantShift(
                                          polyhedron->occurrence()->betaVector(),
                                          verticalDepth,
                                          m_polyhedron->localVerticalMax() * (grainAmount - 1)));
      }
    }
  }

  if (!group.transformations.empty()) {
    if (reverse) grainAmount = -grainAmount;
    // Updating coordinate systems so that the scaled polyhedra fit inside.
    if (m_direction == Dir::RIGHT || m_direction == Dir::LEFT) {
      if (grainAmount > 0 && grainAmount > preGrainAmount) {
        m_polyhedron->coordinateSystem()->projection()->ensureFitsHorizontally(
              m_polyhedron->coordinateSystem(),
              m_polyhedron->localHorizontalMax() + horizontalRange * (grainAmount - preGrainAmount) / preGrainAmount,
              m_polyhedron->localHorizontalMax() + horizontalRange * (grainAmount - preGrainAmount) / preGrainAmount);
      } else if (grainAmount < 0 && grainAmount < -preGrainAmount) {
        m_polyhedron->coordinateSystem()->projection()->ensureFitsHorizontally(
              m_polyhedron->coordinateSystem(),
              m_polyhedron->localHorizontalMin() - horizontalRange * (grainAmount - preGrainAmount) / preGrainAmount,
              m_polyhedron->localHorizontalMin() - horizontalRange * (grainAmount - preGrainAmount) / preGrainAmount);
      }
    }

    if (m_direction == Dir::UP || m_direction == Dir::DOWN) {
      if (grainAmount > 0 && grainAmount > preGrainAmount) {
        m_polyhedron->coordinateSystem()->projection()->ensureFitsVertically(
              m_polyhedron->coordinateSystem(),
              m_polyhedron->localVerticalMax() + verticalRange * (grainAmount - preGrainAmount) / preGrainAmount,
              m_polyhedron->localVerticalMax() + verticalRange * (grainAmount - preGrainAmount) / preGrainAmount);
      } else if (grainAmount < 0 && grainAmount < -preGrainAmount) {
        m_polyhedron->coordinateSystem()->projection()->ensureFitsVertically(
              m_polyhedron->coordinateSystem(),
              m_polyhedron->localVerticalMin() - verticalRange * (grainAmount - preGrainAmount) / preGrainAmount,
              m_polyhedron->localVerticalMin() - verticalRange * (grainAmount - preGrainAmount) / preGrainAmount);
      }
    }

    m_polyhedron->occurrence()->scop()->transform(group);
    m_polyhedron->occurrence()->scop()->executeTransformationSequence();
    m_polyhedron->updateShape();
  } else {
    // Fix to the original position
    m_polyhedron->coordinateSystem()->resetPolyhedronPos(polyhedron);
    m_polyhedron->updateShape();
  }

  m_polyhedron = nullptr;
}

void VizManipulationManager::polyhedronResizing(QPointF displacement) {
  VizProperties *properties = m_polyhedron->coordinateSystem()->projection()->vizProperties();
  const double pointDistance = properties->pointDistance();
  m_horzOffset = round(displacement.x() / pointDistance);
  m_vertOffset = round(displacement.y() / pointDistance);

  bool verticalJoin = (m_direction == Dir::UP && m_vertOffset == 1) ||
                      (m_direction == Dir::DOWN && m_vertOffset == -1);
  bool horizontalJoin = (m_direction == Dir::LEFT && m_horzOffset == 1)  ||
                        (m_direction == Dir::RIGHT && m_horzOffset == -1);

  if (m_creatingDimension == -1 && abs(m_horzOffset) >= 1) {
    m_creatingDimension = 1;
    m_polyhedron->coordinateSystem()->setHorizontalAxisState(VizCoordinateSystem::AxisState::WillAppear);
  } else if (m_creatingDimension == -2 && abs(m_vertOffset) >= 1) {
    m_creatingDimension = 2;
    m_polyhedron->coordinateSystem()->setVerticalAxisState(VizCoordinateSystem::AxisState::WillAppear);
  } else if (m_creatingDimension == 1 && abs(m_horzOffset) == 0) {
    m_creatingDimension = -1;
    m_polyhedron->coordinateSystem()->setHorizontalAxisState(VizCoordinateSystem::AxisState::Invisible);
  } else if (m_creatingDimension == 2 && abs(m_vertOffset) == 0) {
    m_creatingDimension = -2;
    m_polyhedron->coordinateSystem()->setVerticalAxisState(VizCoordinateSystem::AxisState::Invisible);
  } else if (m_creatingDimension == -3 && horizontalJoin) {
    m_creatingDimension = 3;
    m_polyhedron->coordinateSystem()->setHorizontalAxisState(VizCoordinateSystem::AxisState::WillDisappear);
  } else if (m_creatingDimension == -4 && verticalJoin) {
    m_creatingDimension = 4;
    m_polyhedron->coordinateSystem()->setVerticalAxisState(VizCoordinateSystem::AxisState::WillDisappear);
  } else if (m_creatingDimension == 3 && !horizontalJoin) {
    m_creatingDimension = -3;
    m_polyhedron->coordinateSystem()->setHorizontalAxisState(VizCoordinateSystem::AxisState::Visible);
  } else if (m_creatingDimension == 4 && !verticalJoin) {
    m_creatingDimension = -4;
    m_polyhedron->coordinateSystem()->setVerticalAxisState(VizCoordinateSystem::AxisState::Visible);
  }

  if (!m_resizing) {
    return;
  }

  if (displacement.x() != 0 && m_direction == Dir::RIGHT) {
    m_polyhedron->coordinateSystem()->projection()->ensureFitsHorizontally(
          m_polyhedron->coordinateSystem(),
          m_polyhedron->localHorizontalMax() + m_horzOffset,
          m_polyhedron->localHorizontalMax() + m_horzOffset);
    m_polyhedron->prepareExtendRight(displacement.x());
  } else if (displacement.x() != 0 && m_direction == Dir::LEFT) {
    m_polyhedron->coordinateSystem()->projection()->ensureFitsHorizontally(
          m_polyhedron->coordinateSystem(),
          m_polyhedron->localHorizontalMin() + m_horzOffset,
          m_polyhedron->localHorizontalMin() + m_horzOffset);
    m_polyhedron->prepareExtendLeft(displacement.x());
  } else if (displacement.y() != 0 && m_direction == Dir::UP) {
    m_polyhedron->coordinateSystem()->projection()->ensureFitsVertically(
          m_polyhedron->coordinateSystem(),
          m_polyhedron->localVerticalMax() - m_vertOffset,
          m_polyhedron->localVerticalMax() - m_vertOffset);
    m_polyhedron->prepareExtendUp(-displacement.y());
  } else if (displacement.y() != 0 && m_direction == Dir::DOWN) {
    m_polyhedron->coordinateSystem()->projection()->ensureFitsVertically(
          m_polyhedron->coordinateSystem(),
          m_polyhedron->localVerticalMin() - m_vertOffset,
          m_polyhedron->localVerticalMin() - m_vertOffset);
    m_polyhedron->prepareExtendDown(-displacement.y());
  }
}

void VizManipulationManager::polyhedronAboutToSkew(VizPolyhedron *polyhedron, int corner) {
  CLINT_ASSERT(polyhedron != nullptr, "cannot resize null polyhedron");
  m_polyhedron = polyhedron;
  m_corner = corner;

  // Do not allow multiple polyhedra skew yet.  This would require creating a graphicsGroup
  // and adding handles for the group to ensure UI consistency.
  std::unordered_set<VizPolyhedron *> selectedPolyhedra =
      polyhedron->coordinateSystem()->projection()->selectionManager()->selectedPolyhedra();
  CLINT_ASSERT(selectedPolyhedra.size() == 0, "No selection allowed when skewing");

  bool oneDimensional = polyhedron->occurrence()->dimensionality() < polyhedron->coordinateSystem()->verticalDimensionIdx();
  bool zeroDimensional = polyhedron->occurrence()->dimensionality() < polyhedron->coordinateSystem()->horizontalDimensionIdx();
  if (zeroDimensional || oneDimensional) {
    polyhedron->resetPointPositions();
    return;
  }
  m_skewing = true;
}

void VizManipulationManager::polyhedronHasSkewed(VizPolyhedron *polyhedron) {
  CLINT_ASSERT(polyhedron == m_polyhedron, "Wrong polyhedron finished skewing");

  if (!m_skewing)
    return;
  m_skewing = false;

  int horizontalRange = (m_polyhedron->localHorizontalMax() - m_polyhedron->localHorizontalMin());
  int verticalRange   = (m_polyhedron->localVerticalMax() - m_polyhedron->localVerticalMin());

  int horizontalPreShiftAmount = m_corner & C_RIGHT ?
        m_polyhedron->localHorizontalMin() :
        m_polyhedron->localHorizontalMax();
  int verticalPreShiftAmount = m_corner & C_BOTTOM ?
        m_polyhedron->localVerticalMax() :
        m_polyhedron->localVerticalMin();

  int verticalSkewFactor = round(static_cast<double>(-m_vertOffset) / verticalRange);
  int horizontalSkewFactor = round(static_cast<double>(m_horzOffset) / horizontalRange);
  if (verticalSkewFactor == 0 && horizontalSkewFactor == 0) {
    return;
  }

  if (!(m_corner & C_RIGHT)) verticalSkewFactor = -verticalSkewFactor;
  if (m_corner & C_BOTTOM) horizontalSkewFactor = -horizontalSkewFactor;

  int horizontalDimIdx = polyhedron->coordinateSystem()->horizontalDimensionIdx();
  int verticalDimIdx   = polyhedron->coordinateSystem()->verticalDimensionIdx();
  size_t horizontalDepth = horizontalDimIdx != VizProperties::NO_DIMENSION ?
        polyhedron->occurrence()->depth(horizontalDimIdx) :
        VizProperties::NO_DIMENSION;
  size_t verticalDepth   = verticalDimIdx != VizProperties::NO_DIMENSION ?
        polyhedron->occurrence()->depth(verticalDimIdx) :
        VizProperties::NO_DIMENSION;

  TransformationGroup group;
  if (verticalSkewFactor != 0 || horizontalSkewFactor != 0) {
    if (horizontalPreShiftAmount != 0) {
      group.transformations.push_back(Transformation::constantShift(
                                        m_polyhedron->occurrence()->betaVector(),
                                        horizontalDepth,
                                        horizontalPreShiftAmount));
    }
    if (verticalPreShiftAmount != 0) {
      group.transformations.push_back(Transformation::constantShift(
                                        m_polyhedron->occurrence()->betaVector(),
                                        verticalDepth,
                                        verticalPreShiftAmount));
    }

    // vertical
    if (verticalSkewFactor != 0) {
      group.transformations.push_back(Transformation::reshape(
                                        m_polyhedron->occurrence()->betaVector(),
                                        verticalDepth,
                                        polyhedron->coordinateSystem()->horizontalDimensionIdx() + 1,
                                        verticalSkewFactor));
    }
    // horizontal
    if (horizontalSkewFactor != 0) {
      group.transformations.push_back(Transformation::reshape(
                                        m_polyhedron->occurrence()->betaVector(),
                                        horizontalDepth,
                                        polyhedron->coordinateSystem()->verticalDimensionIdx() + 1, // input dim is not subject to tiling
                                        horizontalSkewFactor));
    }

    if (horizontalPreShiftAmount != 0) {
      group.transformations.push_back(Transformation::constantShift(
                                        m_polyhedron->occurrence()->betaVector(),
                                        horizontalDepth,
                                        -horizontalPreShiftAmount));
    }
    if (verticalPreShiftAmount != 0) {
      group.transformations.push_back(Transformation::constantShift(
                                        m_polyhedron->occurrence()->betaVector(),
                                        verticalDepth,
                                        -verticalPreShiftAmount));
    }
  }

  if (!group.transformations.empty()) {
    m_polyhedron->occurrence()->scop()->transform(group);
    m_polyhedron->occurrence()->scop()->executeTransformationSequence();
  } else {
    // Fix to the original position
    m_polyhedron->coordinateSystem()->resetPolyhedronPos(polyhedron);
  }
  m_polyhedron->updateShape();

  m_polyhedron = nullptr;
}

void VizManipulationManager::polyhedronSkewing(QPointF displacement) {
  if (!m_skewing)
    return;

  VizProperties *properties = m_polyhedron->coordinateSystem()->projection()->vizProperties();
  const double pointDistance = properties->pointDistance();
  m_horzOffset = round(displacement.x() / pointDistance);
  m_vertOffset = round(displacement.y() / pointDistance);

  if (displacement.y() != 0) {
    m_polyhedron->coordinateSystem()->projection()->ensureFitsVertically(
          m_polyhedron->coordinateSystem(),
          m_polyhedron->localVerticalMin() - m_vertOffset,
          m_polyhedron->localVerticalMax() - m_vertOffset);
    if (m_corner & C_RIGHT) {
      m_polyhedron->prepareSkewVerticalRight(-displacement.y());
    } else {
      m_polyhedron->prepareSkewVerticalLeft(-displacement.y());
    }
  }
  if (displacement.x() != 0) {
    m_polyhedron->coordinateSystem()->projection()->ensureFitsHorizontally(
          m_polyhedron->coordinateSystem(),
          m_polyhedron->localHorizontalMin() + m_horzOffset,
          m_polyhedron->localHorizontalMax() + m_horzOffset);
    if (m_corner & C_BOTTOM) {
      m_polyhedron->prepareSkewHorizontalBottom(displacement.x());
    } else {
      m_polyhedron->prepareSkewHorizontalTop(displacement.x());
    }
  }
}

void VizManipulationManager::polyhedronAboutToRotate(VizPolyhedron *polyhedron, int corner) {
  CLINT_ASSERT(polyhedron != nullptr, "Cannot rotate null polyhedron");
  m_polyhedron = polyhedron;
  m_corner = corner;
  ensureTargetConsistency();
  m_rotationAngle = 0;

  int hmin, hmax, vmin, vmax;
  m_polyhedron->coordinateSystem()->minMax(hmin, hmax, vmin, vmax);
  int coordMin = std::min(hmin, vmin);
  int coordMax = std::max(hmax, vmax);
  m_polyhedron->coordinateSystem()->projection()->ensureFitsHorizontally(
        m_polyhedron->coordinateSystem(), coordMin, coordMax);
  m_polyhedron->coordinateSystem()->projection()->ensureFitsVertically(
        m_polyhedron->coordinateSystem(), coordMin, coordMax);
}

int VizManipulationManager::rotationCase()
{
  if (m_rotationAngle < 0) {
    m_rotationAngle += 360 * ceil(-m_rotationAngle / 360.0);
  } else if (m_rotationAngle >= 360) {
    m_rotationAngle -= 360 * floor(m_rotationAngle / 360.0);
  }
  int rotateCase = static_cast<int>(round(m_rotationAngle / 90.0));
  rotateCase = rotateCase % 4;

  return rotateCase;
}

void VizManipulationManager::polyhedronHasRotated(VizPolyhedron *polyhedron) {
  CLINT_ASSERT(polyhedron == m_polyhedron, "Wrong polyhedron finished rotating");

  TransformationGroup group;

  size_t horizontalDimIdx = polyhedron->coordinateSystem()->horizontalDimensionIdx();
  size_t verticalDimIdx   = polyhedron->coordinateSystem()->verticalDimensionIdx();
  size_t horizontalDepth = horizontalDimIdx != VizProperties::NO_DIMENSION ?
        polyhedron->occurrence()->depth(horizontalDimIdx) :
        VizProperties::NO_DIMENSION;
  size_t verticalDepth   = verticalDimIdx != VizProperties::NO_DIMENSION ?
        polyhedron->occurrence()->depth(verticalDimIdx) :
        VizProperties::NO_DIMENSION;

  int rotateCase = rotationCase();
  m_polyhedron->prepareRotateAngle(rotateCase * -90.0);
  if (rotateCase == 1) {
    group.transformations.push_back(Transformation::reverse(
                                      m_polyhedron->occurrence()->betaVector(),
                                      verticalDepth));
    if (m_polyhedron->localVerticalMin() + m_polyhedron->localVerticalMax() != 0)
      group.transformations.push_back(Transformation::constantShift(
                                        m_polyhedron->occurrence()->betaVector(),
                                        verticalDepth,
                                        -m_polyhedron->localVerticalMin() - m_polyhedron->localVerticalMax()));
    group.transformations.push_back(Transformation::interchange(
                                      m_polyhedron->occurrence()->betaVector(),
                                      horizontalDepth,
                                      verticalDepth));
  } else if (rotateCase == 2) {
    group.transformations.push_back(Transformation::reverse(
                                      m_polyhedron->occurrence()->betaVector(),
                                      horizontalDepth));
    if (m_polyhedron->localHorizontalMin() + m_polyhedron->localHorizontalMax() != 0)
      group.transformations.push_back(Transformation::constantShift(
                                        m_polyhedron->occurrence()->betaVector(),
                                        horizontalDepth,
                                        -m_polyhedron->localHorizontalMin() - m_polyhedron->localHorizontalMax()));
    group.transformations.push_back(Transformation::reverse(
                                      m_polyhedron->occurrence()->betaVector(),
                                      verticalDepth));
    if (m_polyhedron->localVerticalMin() + m_polyhedron->localVerticalMax() != 0)
      group.transformations.push_back(Transformation::constantShift(
                                        m_polyhedron->occurrence()->betaVector(),
                                        verticalDepth,
                                        -m_polyhedron->localVerticalMin() - m_polyhedron->localVerticalMax()));
  } else if (rotateCase == 3) {
    group.transformations.push_back(Transformation::reverse(
                                      m_polyhedron->occurrence()->betaVector(),
                                      horizontalDepth));
    if (m_polyhedron->localHorizontalMin() + m_polyhedron->localHorizontalMax() != 0)
      group.transformations.push_back(Transformation::constantShift(
                                        m_polyhedron->occurrence()->betaVector(),
                                        horizontalDepth,
                                        -m_polyhedron->localHorizontalMin() - m_polyhedron->localHorizontalMax()));
    group.transformations.push_back(Transformation::interchange(
                                      m_polyhedron->occurrence()->betaVector(),
                                      horizontalDepth,
                                      verticalDepth));
  } else if (rotateCase == 0) {
    // Do nothing
  } else {
    CLINT_UNREACHABLE;
  }

  if (!group.transformations.empty()) {
    m_polyhedron->occurrence()->scop()->transform(group);
    m_polyhedron->occurrence()->scop()->executeTransformationSequence();
  } else {
    // Fix to the original position
    m_polyhedron->coordinateSystem()->resetPolyhedronPos(polyhedron);
    m_polyhedron->updateShape();
  }

  m_polyhedron = nullptr;
}

void VizManipulationManager::polyhedronRotating(QPointF displacement) {
  if (!displacement.isNull())
    m_rotationAngle = m_polyhedron->prepareRotate(displacement);
}

void VizManipulationManager::pointRightClicked(VizPoint *point) {
  if (point == nullptr)
    return;

  const std::unordered_set<VizPoint *> &selectedPoints =
      point->coordinateSystem()->projection()->selectionManager()->selectedPoints();

  // FIXME: the selection manager should provide information on the polygon enclosing
  // selected points, rather than straightforward computation in manipulation manager
  // This is also the case for index-set splitting (some functionality is in occurrence).
  bool samePolyhedron = true;
  int selectionHorizontalMin = INT_MAX,
      selectionHorizontalMax = INT_MIN,
      selectionVerticalMin   = INT_MAX,
      selectionVerticalMax   = INT_MIN;
  std::set<std::pair<int, int>> scatteredCoords;
  for (VizPoint *pt : selectedPoints) {
    int horzCoordinate, vertCoordinate;
    std::tie(horzCoordinate, vertCoordinate) = pt->scatteredCoordinates();

    selectionHorizontalMin = std::min(selectionHorizontalMin, horzCoordinate);
    selectionHorizontalMax = std::max(selectionHorizontalMax, horzCoordinate);
    selectionVerticalMin   = std::min(selectionVerticalMin, vertCoordinate);
    selectionVerticalMax   = std::max(selectionVerticalMax, vertCoordinate);

    scatteredCoords.insert(pt->scatteredCoordinates());

    samePolyhedron = samePolyhedron && (pt->polyhedron() == point->polyhedron());
  }

  if (!samePolyhedron)
    return;

  // Check that this coordinate system is the only one in its pile.
  size_t pileIndex;
  std::tie(pileIndex, std::ignore) = point->coordinateSystem()->projection()->csIndices(point->coordinateSystem());
  bool onlyCS = point->coordinateSystem()->projection()->pileCSNumber(pileIndex) == 1;

  ClintStmtOccurrence *occurrence = point->polyhedron()->occurrence();

  int horizontalTileSize = selectionHorizontalMax - selectionHorizontalMin + 1;
  int verticalTileSize   = selectionVerticalMax - selectionVerticalMin + 1;

  // The selection is a perfect rectangle (do not know what to do with triangles yet).
  // All points have unique coordinates, and cover the whole space defined by min/max
  if (scatteredCoords.size() != selectedPoints.size() ||
      scatteredCoords.size() != horizontalTileSize * verticalTileSize)
    return;

  // TODO: shift the polyhedron if the selection does not start at its beginning so that
  // the selection would match the tile.

  int horizontalOccurrenceSpan = occurrence->maximumValue(point->coordinateSystem()->horizontalDimensionIdx())
      - occurrence->minimumValue(point->coordinateSystem()->horizontalDimensionIdx()) + 1;
  int verticalOccurrenceSpan = occurrence->maximumValue(point->coordinateSystem()->verticalDimensionIdx())
      - occurrence->minimumValue(point->coordinateSystem()->verticalDimensionIdx()) + 1;

  bool horizontalTilingAllowed = onlyCS && point->coordinateSystem()->isHorizontalAxisVisible();
  bool horizontalFullSelection = horizontalTileSize == horizontalOccurrenceSpan;
  bool horizontalTiled = occurrence->isProjectionTiled(point->coordinateSystem()->horizontalDimensionIdx());
  int horizontalTileSizeOld = occurrence->projectionDimTileSize(point->coordinateSystem()->horizontalDimensionIdx());

  bool verticalTilingAllowed   = point->coordinateSystem()->isVerticalAxisVisible();
  bool verticalFullSelection = verticalTileSize == verticalOccurrenceSpan;
  bool verticalTiled = occurrence->isProjectionTiled(point->coordinateSystem()->verticalDimensionIdx());
  int verticalTileSizeOld = occurrence->projectionDimTileSize(point->coordinateSystem()->verticalDimensionIdx());

  TransformationGroup group;
  if (verticalTilingAllowed && verticalTileSizeOld != verticalTileSize) {
    int verticalDepth = occurrence->depth(point->coordinateSystem()->verticalDimensionIdx());
    std::vector<int> beta = occurrence->betaVector();
    beta.resize(verticalDepth);
    if (verticalTiled) {
      group.transformations.push_back(Transformation::linearize(
                                        beta,
                                        verticalDepth - 1));
      occurrence->scop()->untile(beta, point->coordinateSystem()->verticalDimensionIdx());
    }
    verticalDepth = occurrence->depth(point->coordinateSystem()->verticalDimensionIdx());
    beta = occurrence->betaVector();
    beta.resize(verticalDepth);
    if (!verticalFullSelection) {
      group.transformations.push_back(Transformation::tile(
                                        beta,
                                        verticalDepth,
                                        verticalTileSize));
      occurrence->scop()->tile(beta,
                               point->coordinateSystem()->verticalDimensionIdx(),
                               verticalTileSize);
    }
  }

  if (horizontalTilingAllowed && horizontalTileSizeOld != horizontalTileSize) {
    int horizontalDepth = occurrence->depth(point->coordinateSystem()->horizontalDimensionIdx());
    std::vector<int> beta = occurrence->betaVector();
    beta.resize(horizontalDepth);
    if (horizontalTiled) {
      group.transformations.push_back(Transformation::linearize(
                                        beta,
                                        horizontalDepth - 1));
      occurrence->scop()->untile(beta, point->coordinateSystem()->horizontalDimensionIdx());
    }
    horizontalDepth = occurrence->depth(point->coordinateSystem()->horizontalDimensionIdx());
    beta = occurrence->betaVector();
    beta.resize(horizontalDepth);
    if (!horizontalFullSelection) {
      group.transformations.push_back(Transformation::tile(
                                        beta,
                                        horizontalDepth,
                                        horizontalTileSize));
      occurrence->scop()->tile(beta,
                               point->coordinateSystem()->horizontalDimensionIdx(),
                               horizontalTileSize);
    }
  }

  if (!group.transformations.empty()) {
    occurrence->scop()->transform(group);
    occurrence->scop()->executeTransformationSequence();
    point->coordinateSystem()->update();
  }
}
