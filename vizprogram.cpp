#include "macros.h"
#include "oslutils.h"
#include "vizprogram.h"
#include "vizscop.h"

VizProgram::VizProgram(osl_scop_p scop, QObject *parent) :
  QObject(parent), m_scop(scop) {

  // TODO: factory that concentrates osl-related creation?
  // AZ: I think it is okay for general objects (program, scop, stmt, stmtoccurence) to use osl.
  // They may accept a different constructor as long as they provide the same interface for the
  // coordinate system part.
  oslListForeach(scop, [this](osl_scop_p sc) {
    VizScop *vizScop = new VizScop(sc, this);
    m_scops.push_back(vizScop);
  });

  m_enumerator = new ISLEnumerator;
}

VizProgram::~VizProgram() {
  delete m_enumerator;
}
