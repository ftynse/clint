#include "macros.h"
#include "oslutils.h"
#include "vizprogram.h"
#include "vizscop.h"

#include <isl/ctx.h>
#include <isl/set.h>
#include <isl/map.h>

#include <osl/relation.h>

#include <piplib/piplibMP.h>

#include <tuple>
#include <functional>
#include <utility>


template <typename T>
T *osl_list_prev(T *var, T *container) {
  T *t;
  for (t = container; t != nullptr; t = t->next) {
    if (t->next == var) {
      break;
    }
  }
  return t;
}

VizProgram::VizProgram(osl_scop_p scop, QObject *parent) :
  QObject(parent) {

  osl_scop_p sc;
  LL_FOREACH(sc, scop) {
    VizScop *vizScop = new VizScop(scop, this);
    scops_.push_back(vizScop);
  }

  osl_names_p names = osl_scop_names(scop);
}
