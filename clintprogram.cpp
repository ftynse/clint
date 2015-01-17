#include "macros.h"
#include "oslutils.h"
#include "clintprogram.h"
#include "clintscop.h"

ClintProgram::ClintProgram(osl_scop_p scop, char *originalCode, QObject *parent) :
  QObject(parent), m_scop(scop) {

  // TODO: factory that concentrates osl-related creation?
  // AZ: I think it is okay for general objects (program, scop, stmt, stmtoccurence) to use osl.
  // They may accept a different constructor as long as they provide the same interface for the
  // coordinate system part.
  oslListForeach(scop, [this,originalCode](osl_scop_p sc) {
    ClintScop *vizScop = new ClintScop(sc, 6, originalCode, this); // FIXME: hardcoded value
    m_scops.push_back(vizScop);
  });

  m_enumerator = new ISLEnumerator;
}

ClintProgram::~ClintProgram() {
  delete m_enumerator;
}
