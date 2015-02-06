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
      int horzDepth = vp->coordinateSystem()->horizontalDimensionIdx() + 1;
      int horzAmount = m_horzOffset;
      int vertDepth = vp->coordinateSystem()->verticalDimensionIdx() + 1;
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
      ClayBetaMapper *mapper = new ClayBetaMapper(polyhedron->scop());
      mapper->apply(nullptr, iterGroup);
      bool happy = true;
      std::map<std::vector<int>, std::vector<int>> mapping;
      for (ClintStmt *stmt : polyhedron->scop()->statements()) {
        for (ClintStmtOccurrence *occurrence : stmt->occurences()) {
          int result;
          std::vector<int> beta = occurrence->betaVector();
          std::vector<int> updatedBeta;
          std::tie(updatedBeta, result) = mapper->map(occurrence->betaVector());

          qDebug() << result << QVector<int>::fromStdVector(beta) << "->" << QVector<int>::fromStdVector(updatedBeta);
          if (result == ClayBetaMapper::SUCCESS &&
              beta != updatedBeta) {
            occurrence->resetBetaVector(updatedBeta);
            mapping[beta] = updatedBeta;
          }
          happy = happy && result == ClayBetaMapper::SUCCESS;
        }
      }
      delete mapper;
      CLINT_ASSERT(happy, "Beta mapping failed");

      polyhedron->scop()->updateBetas(mapping);

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
    polyhedron->scop()->transform(group);
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

  std::map<int, std::set<int>> horizontal, vertical;
  bool samePolyhedron = true;
  for (VizPoint *pt : selectedPoints) {
    int horzCoordinate, vertCoordinate;
    std::tie(horzCoordinate, vertCoordinate) = pt->scatteredCoordinates();
    horizontal[horzCoordinate].emplace(vertCoordinate);
    vertical[vertCoordinate].emplace(horzCoordinate);
    samePolyhedron = samePolyhedron && (pt->polyhedron() == point->polyhedron());
  }

  if (!samePolyhedron)
    return;

  int horizontalSize = point->polyhedron()->localHorizontalMax() -
      point->polyhedron()->localHorizontalMin();
  int verticalSize = point->polyhedron()->localVerticalMax() -
      point->polyhedron()->localVerticalMin();

  // Condition 1: adjacent to one of borders and consecutive (w/o localdim)
  // std::map is ordered by key
  // Condition 2: all points in line selected (w/o localdim; fails for triangles)
  int horizontalStart = (horizontal.begin())->first;
  int horizontalEnd   = (horizontal.rbegin())->first;
  int verticalStart   = (vertical.begin())->first;
  int verticalEnd     = (vertical.rbegin())->first;
  if (horizontalStart == point->polyhedron()->localHorizontalMin() && // Attached to the horizontal beginning
      horizontalEnd - horizontalStart == horizontal.size() - 1 &&     // All consecutive lines selected
      horizontalEnd - horizontalStart != horizontalSize) {            // Not all lines selected
    bool all = std::all_of(std::begin(horizontal), std::end(horizontal), [verticalSize,point](const std::pair<int, std::set<int>> &pair) {
      const std::set<int> &s = pair.second;
      return s.size() == verticalSize + 1 &&
          *(s.begin()) == point->polyhedron()->localVerticalMin() &&
          *(s.rbegin()) == point->polyhedron()->localVerticalMax();
    });
    if (all) {
      // Vertical (inner) iss
      m_pointDetachState = PT_DETACH_VERTICAL;
      m_pointDetachValue = point->polyhedron()->localVerticalMin() + (horizontalEnd - horizontalStart + 1);
      m_pointDetachLast = false;
    }
  } else if (horizontalEnd == point->polyhedron()->localHorizontalMax() &&
             horizontalEnd - horizontalStart == horizontal.size() - 1 &&
             horizontalEnd - horizontalStart != horizontalSize) {

    bool all = std::all_of(std::begin(horizontal), std::end(horizontal), [verticalSize,point](const std::pair<int, std::set<int>> &pair) {
      const std::set<int> &s = pair.second;
      return s.size() == verticalSize + 1 &&
          *(s.begin()) == point->polyhedron()->localVerticalMin() &&
          *(s.rbegin()) == point->polyhedron()->localVerticalMax();
    });
    if (all) {
      // Vertical (inner) iss
      m_pointDetachState = PT_DETACH_VERTICAL;
      m_pointDetachValue = point->polyhedron()->localVerticalMin() + (horizontalEnd - horizontalStart + 1);
      m_pointDetachLast = true;
    }

  } else if (verticalStart == point->polyhedron()->localVerticalMin() &&
             verticalEnd - verticalStart == vertical.size() - 1 &&
             verticalEnd - verticalStart != verticalSize) {
    bool all = std::all_of(std::begin(vertical), std::end(vertical), [horizontalSize,point](const std::pair<int, std::set<int>> &pair) {
      const std::set<int> &s = pair.second;
      return s.size() == horizontalSize + 1 &&
          *(s.begin()) == point->polyhedron()->localHorizontalMin() &&
          *(s.rbegin()) == point->polyhedron()->localHorizontalMax();
    });
    if (all) {
      m_pointDetachState = PT_DETACH_HORIZONTAL;
      m_pointDetachValue = point->polyhedron()->localHorizontalMin() + (verticalEnd - verticalStart + 1);
      m_pointDetachLast = false;
    }
  } else if (verticalEnd == point->polyhedron()->localHorizontalMax() &&
             verticalEnd - verticalStart == vertical.size() - 1 &&
             verticalEnd - verticalStart != verticalSize) {
    bool all = std::all_of(std::begin(vertical), std::end(vertical), [horizontalSize,point](const std::pair<int, std::set<int>> &pair) {
      const std::set<int> &s = pair.second;
      return s.size() == horizontalSize + 1 &&
          *(s.begin()) == point->polyhedron()->localHorizontalMin() &&
          *(s.rbegin()) == point->polyhedron()->localHorizontalMax();
    });
    if (all) {
      m_pointDetachState = PT_DETACH_HORIZONTAL;
      m_pointDetachValue = point->polyhedron()->localHorizontalMin() + (verticalEnd - verticalStart + 1);
      m_pointDetachLast = true;
    }
  } else {
    m_pointDetachState = PT_NODETACH;
    return; // no transformation;
    // TODO: if everything selected -> select the polyhedron; do not allow ISSing all points
  }

}

void VizManipulationManager::pointHasMoved(VizPoint *point) {
  switch (m_pointDetachState) {
  case PT_NODETACH:
  case PT_DETACH_VERTICAL:
  case PT_DETACH_HORIZONTAL:
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
    // FIXME: 2d only
    VizPolyhedron *polyhedron = new VizPolyhedron(nullptr, point->coordinateSystem());
    VizSelectionManager *sm = point->coordinateSystem()->projection()->selectionManager();
    const std::unordered_set<VizPoint *> selectedPoints = sm->selectedPoints();
    CLINT_ASSERT(std::find(std::begin(selectedPoints), std::end(selectedPoints), point) != std::end(selectedPoints),
                 "The point being manipulated is not selected");

    VizPolyhedron *oldPolyhedron = point->polyhedron();

    int dimensionIdx;
    int minValue;
    if (m_pointDetachState == PT_DETACHED_HORIZONTAL) {
      dimensionIdx = oldPolyhedron->coordinateSystem()->verticalDimensionIdx();
      minValue = oldPolyhedron->localVerticalMin();
    } else {
      dimensionIdx = oldPolyhedron->coordinateSystem()->horizontalDimensionIdx();
      minValue = oldPolyhedron->localHorizontalMin();
    }

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
    if (m_pointDetachLast) {
      group.transformations.push_back(
            Transformation::issLast(betaLoop, dimensionIdx, minValue + m_pointDetachValue));
    } else {
      group.transformations.push_back(
            Transformation::issFirst(betaLoop, dimensionIdx, minValue + m_pointDetachValue));
    }

    Transformer *c = new ClayScriptGenerator(std::cerr);
    c->apply(nullptr, group);
    delete c;

    oldPolyhedron->scop()->transform(group);
    oldPolyhedron->scop()->executeTransformationSequence();

    betaLoop.push_back(1); // FIXME: works only if one polygon in CS
    ClintStmtOccurrence *occurrence = oldPolyhedron->scop()->occurrence(betaLoop);
    polyhedron->setOccurrenceSilent(occurrence);
    oldPolyhedron->updateInternalDependences();
    polyhedron->updateInternalDependences();
    polyhedron->coordinateSystem()->projection()->updateInnerDependences();
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
}

void VizManipulationManager::polyhedronHasResized(VizPolyhedron *polyhedron) {
  CLINT_ASSERT(m_polyhedron == polyhedron, "Wrong polyhedron finished resizing");

  bool isHorizontal;
  if (m_direction == Dir::LEFT || m_direction == Dir::RIGHT) {
    isHorizontal = true;
  } else if (m_direction == Dir::UP || m_direction == Dir::DOWN) {
    isHorizontal = false;
  } else {
    CLINT_UNREACHABLE;
  }

  int horizontalRange = (m_polyhedron->localHorizontalMax() - m_polyhedron->localHorizontalMin());
  int verticalRange   = (m_polyhedron->localVerticalMax() - m_polyhedron->localVerticalMin());
  int grainAmount;
  switch (m_direction) {
  case Dir::LEFT:
    grainAmount = -round(static_cast<double>(m_horzOffset) / horizontalRange) + 1;
    break;
  case Dir::RIGHT:
    grainAmount = round(static_cast<double>(m_horzOffset) / horizontalRange) + 1;
    break;
  case Dir::UP:
    grainAmount = -round(static_cast<double>(m_vertOffset) / verticalRange) + 1;
    break;
  case Dir::DOWN:
    grainAmount = round(static_cast<double>(m_vertOffset) / verticalRange) + 1;
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

  if (grainAmount > 1) {
    if (m_direction == Dir::LEFT || m_direction == Dir::RIGHT) {
      group.transformations.push_back(Transformation::grain(
                                        polyhedron->occurrence()->betaVector(),
                                        polyhedron->coordinateSystem()->horizontalDimensionIdx() + 1,
                                        grainAmount));

      if (m_polyhedron->localHorizontalMin() != 0 && m_direction == Dir::RIGHT) {
        group.transformations.push_back(Transformation::constantShift(
                                          polyhedron->occurrence()->betaVector(),
                                          polyhedron->coordinateSystem()->horizontalDimensionIdx() + 1,
                                          m_polyhedron->localHorizontalMin() * (grainAmount - 1)));
      } else if (m_polyhedron->localHorizontalMax() != 0 && m_direction == Dir::LEFT) {
        group.transformations.push_back(Transformation::constantShift(
                                          polyhedron->occurrence()->betaVector(),
                                          polyhedron->coordinateSystem()->horizontalDimensionIdx() + 1,
                                          m_polyhedron->localHorizontalMax() * (grainAmount - 1)));
      }
    } else if (m_direction == Dir::DOWN || m_direction == Dir::UP) {
      group.transformations.push_back(Transformation::grain(
                                        polyhedron->occurrence()->betaVector(),
                                        polyhedron->coordinateSystem()->verticalDimensionIdx() + 1,
                                        grainAmount));

      if (m_polyhedron->localVerticalMin() != 0 && m_direction == Dir::UP) {
        group.transformations.push_back(Transformation::constantShift(
                                          polyhedron->occurrence()->betaVector(),
                                          polyhedron->coordinateSystem()->verticalDimensionIdx() + 1,
                                          m_polyhedron->localVerticalMin() * (grainAmount - 1)));
      } else if (m_polyhedron->localVerticalMax() != 0 && m_direction == Dir::DOWN) {
        group.transformations.push_back(Transformation::constantShift(
                                          polyhedron->occurrence()->betaVector(),
                                          polyhedron->coordinateSystem()->verticalDimensionIdx() + 1,
                                          m_polyhedron->localVerticalMax() * (grainAmount - 1)));
      }
    }
  }

  if (!group.transformations.empty()) {
    if (reverse) grainAmount = -grainAmount;
    // Updating coordinate systems so that the scaled polyhedra fit inside.
    if (m_direction == Dir::RIGHT) {
      m_polyhedron->coordinateSystem()->projection()->ensureFitsHorizontally(
            m_polyhedron->coordinateSystem(),
            m_polyhedron->localHorizontalMax() + horizontalRange * (grainAmount - 1),
            m_polyhedron->localHorizontalMax() + horizontalRange * (grainAmount - 1));
    } else if (m_direction == Dir::LEFT) {
      m_polyhedron->coordinateSystem()->projection()->ensureFitsHorizontally(
            m_polyhedron->coordinateSystem(),
            m_polyhedron->localHorizontalMin() - horizontalRange * (grainAmount - 1),
            m_polyhedron->localHorizontalMin() - horizontalRange * (grainAmount - 1));
    }

    if (m_direction == Dir::UP) {
      m_polyhedron->coordinateSystem()->projection()->ensureFitsVertically(
            m_polyhedron->coordinateSystem(),
            m_polyhedron->localVerticalMax() + verticalRange * (grainAmount - 1),
            m_polyhedron->localVerticalMax() + verticalRange * (grainAmount - 1));
    } else if (m_direction == Dir::DOWN) {
      m_polyhedron->coordinateSystem()->projection()->ensureFitsVertically(
            m_polyhedron->coordinateSystem(),
            m_polyhedron->localVerticalMin() - verticalRange * (grainAmount - 1),
            m_polyhedron->localVerticalMin() - verticalRange * (grainAmount - 1));
    }

    m_polyhedron->occurrence()->scop()->transform(group);
    m_polyhedron->occurrence()->scop()->executeTransformationSequence();
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

  // Skewings are not combinable as we think they are.
  const TransformationSequence &sequence = polyhedron->scop()->transformationSequence();
  auto iterator = std::find_if(std::begin(sequence.groups), std::end(sequence.groups),
                               [](const TransformationGroup &group) {
    return std::any_of(std::begin(group.transformations), std::end(group.transformations),
                [](const Transformation &transformation) {
      return transformation.kind() == Transformation::Kind::Skew;
    });
  });
  if (iterator != std::end(sequence.groups)) {
    CLINT_WARNING(false, "Trying to combine skew transformations");
    return;
  }

  m_skewing = true;

  // Do not allow multiple polyhedra resize yet.  This would require creating a graphicsGroup
  // and adding handles for the group to ensure UI consistency.
  std::unordered_set<VizPolyhedron *> selectedPolyhedra =
      polyhedron->coordinateSystem()->projection()->selectionManager()->selectedPolyhedra();
  CLINT_ASSERT(selectedPolyhedra.size() == 0, "No selection allowed when resizing");
}

void VizManipulationManager::polyhedronHasSkewed(VizPolyhedron *polyhedron) {
  CLINT_ASSERT(polyhedron == m_polyhedron, "Wrong polyhedron finished skewing");

  int horizontalRange = (m_polyhedron->localHorizontalMax() - m_polyhedron->localHorizontalMin());
  int verticalRange   = (m_polyhedron->localVerticalMax() - m_polyhedron->localVerticalMin());

  int horizontalPreShiftAmount = m_corner & C_RIGHT ?
        m_polyhedron->localHorizontalMin() :
        m_polyhedron->localHorizontalMax();
  int verticalPreShiftAmount = m_corner & C_BOTTOM ?
        m_polyhedron->localVerticalMax() :
        m_polyhedron->localVerticalMin();

  // TODO: verify that this decomposition actually works...
  // It does not, skew transformations are not trivially combinable
  int verticalSkewFactor = round(static_cast<double>(-m_vertOffset) / verticalRange);
  int horizontalSkewFactor = round(static_cast<double>(m_horzOffset) / horizontalRange);

  if (!m_skewing) { // workaround to prevent unwanted (second+) skewing
    verticalSkewFactor = 0;
    horizontalSkewFactor = 0;
  }
  m_skewing = false;

  if (!(m_corner & C_RIGHT)) verticalSkewFactor = -verticalSkewFactor;
  if (m_corner & C_BOTTOM) horizontalSkewFactor = -horizontalSkewFactor;

  TransformationGroup group;
  if (verticalSkewFactor != 0 || horizontalSkewFactor != 0) {
    if (horizontalPreShiftAmount != 0) {
      group.transformations.push_back(Transformation::constantShift(
                                        m_polyhedron->occurrence()->betaVector(),
                                        m_polyhedron->coordinateSystem()->horizontalDimensionIdx() + 1,
                                        horizontalPreShiftAmount));
    }
    if (verticalPreShiftAmount != 0) {
      group.transformations.push_back(Transformation::constantShift(
                                        m_polyhedron->occurrence()->betaVector(),
                                        m_polyhedron->coordinateSystem()->verticalDimensionIdx() + 1,
                                        verticalPreShiftAmount));
    }

    // vertical
    if (verticalSkewFactor != 0) {
      group.transformations.push_back(Transformation::skew(
                                        m_polyhedron->occurrence()->betaVector(),
                                        m_polyhedron->coordinateSystem()->horizontalDimensionIdx() + 1,
                                        m_polyhedron->coordinateSystem()->verticalDimensionIdx() + 1,
                                        verticalSkewFactor));
    }
    // horizontal
    if (horizontalSkewFactor != 0) {
      group.transformations.push_back(Transformation::skew(
                                        m_polyhedron->occurrence()->betaVector(),
                                        m_polyhedron->coordinateSystem()->verticalDimensionIdx() + 1,
                                        m_polyhedron->coordinateSystem()->horizontalDimensionIdx() + 1,
                                        horizontalSkewFactor));
    }

    if (horizontalPreShiftAmount != 0) {
      group.transformations.push_back(Transformation::constantShift(
                                        m_polyhedron->occurrence()->betaVector(),
                                        m_polyhedron->coordinateSystem()->horizontalDimensionIdx() + 1,
                                        -horizontalPreShiftAmount));
    }
    if (verticalPreShiftAmount != 0) {
      group.transformations.push_back(Transformation::constantShift(
                                        m_polyhedron->occurrence()->betaVector(),
                                        m_polyhedron->coordinateSystem()->verticalDimensionIdx() + 1,
                                        -verticalPreShiftAmount));
    }
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

void VizManipulationManager::polyhedronSkewing(QPointF displacement) {
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

  int rotateCase = rotationCase();
  m_polyhedron->prepareRotateAngle(rotateCase * -90.0);
  if (rotateCase == 1) {
    group.transformations.push_back(Transformation::reverse(
                                      m_polyhedron->occurrence()->betaVector(),
                                      m_polyhedron->coordinateSystem()->verticalDimensionIdx() + 1));
    if (m_polyhedron->localVerticalMin() + m_polyhedron->localVerticalMax() != 0)
      group.transformations.push_back(Transformation::constantShift(
                                        m_polyhedron->occurrence()->betaVector(),
                                        m_polyhedron->coordinateSystem()->verticalDimensionIdx() + 1,
                                        -m_polyhedron->localVerticalMin() - m_polyhedron->localVerticalMax()));
    group.transformations.push_back(Transformation::interchange(
                                      m_polyhedron->occurrence()->betaVector(),
                                      m_polyhedron->coordinateSystem()->horizontalDimensionIdx() + 1,
                                      m_polyhedron->coordinateSystem()->verticalDimensionIdx() + 1));
  } else if (rotateCase == 2) {
    group.transformations.push_back(Transformation::reverse(
                                      m_polyhedron->occurrence()->betaVector(),
                                      m_polyhedron->coordinateSystem()->horizontalDimensionIdx() + 1));
    if (m_polyhedron->localHorizontalMin() + m_polyhedron->localHorizontalMax() != 0)
      group.transformations.push_back(Transformation::constantShift(
                                        m_polyhedron->occurrence()->betaVector(),
                                        m_polyhedron->coordinateSystem()->horizontalDimensionIdx() + 1,
                                        -m_polyhedron->localHorizontalMin() - m_polyhedron->localHorizontalMax()));
    group.transformations.push_back(Transformation::reverse(
                                      m_polyhedron->occurrence()->betaVector(),
                                      m_polyhedron->coordinateSystem()->verticalDimensionIdx() + 1));
    if (m_polyhedron->localVerticalMin() + m_polyhedron->localVerticalMax() != 0)
      group.transformations.push_back(Transformation::constantShift(
                                        m_polyhedron->occurrence()->betaVector(),
                                        m_polyhedron->coordinateSystem()->verticalDimensionIdx() + 1,
                                        -m_polyhedron->localVerticalMin() - m_polyhedron->localVerticalMax()));
  } else if (rotateCase == 3) {
    group.transformations.push_back(Transformation::reverse(
                                      m_polyhedron->occurrence()->betaVector(),
                                      m_polyhedron->coordinateSystem()->horizontalDimensionIdx() + 1));
    if (m_polyhedron->localHorizontalMin() + m_polyhedron->localHorizontalMax() != 0)
      group.transformations.push_back(Transformation::constantShift(
                                        m_polyhedron->occurrence()->betaVector(),
                                        m_polyhedron->coordinateSystem()->horizontalDimensionIdx() + 1,
                                        -m_polyhedron->localHorizontalMin() - m_polyhedron->localHorizontalMax()));
    group.transformations.push_back(Transformation::interchange(
                                      m_polyhedron->occurrence()->betaVector(),
                                      m_polyhedron->coordinateSystem()->horizontalDimensionIdx() + 1,
                                      m_polyhedron->coordinateSystem()->verticalDimensionIdx() + 1));
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
