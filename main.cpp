#include <QtWidgets>
#include <QApplication>

#include "clintwindow.h"

#include "enumerator.h"
#include "oslutils.h"
#include <cassert>

namespace {

void enumerationTest() {
  Enumerator *enumerator = new ISLEnumerator;
  FILE *file = fopen("enumeration.scop", "r");
  assert(file);
  osl_scop_p scop = osl_scop_read(file);
  fclose(file);
  osl_statement_p statement;
//  osl_relation_p fixedParamsContext = oslRelationsFixAllParameters(scop->context, 4);
  osl_relation_p fixedParamsContext = osl_relation_clone(scop->context);
  LL_FOREACH(statement, scop->statement) {
    osl_relation_p relation = oslApplyScattering(statement);
    osl_relation_p ready = oslRelationsWithContext(relation, fixedParamsContext);
    osl_relation_free(relation);
    std::vector<int> dims;
    dims.reserve(ready->nb_output_dims);
    for (int i = 0; i < ready->nb_output_dims; i++) {
      dims.push_back(i);
    }
    osl_relation_p ready_part;
    LL_FOREACH(ready_part, ready) {
      osl_relation_p keeper = ready_part->next;
      ready_part->next = nullptr;
      auto points = enumerator->enumerate(ready_part, dims);
      ready_part->next = keeper;
      std::for_each(std::begin(points), std::end(points), [](std::vector<int> &p){
        std::copy(std::begin(p), std::end(p), std::ostream_iterator<int>(std::cout, ", "));
        std::cout << std::endl;
      });
    }
  }
  delete enumerator;
}

} // end anonymous namespace

int main(int argc, char **argv)
{
  QApplication app(argc, argv);
  ClintWindow window;
  window.showMaximized();

  if (app.arguments().contains("--test=enumeration")) {
    enumerationTest();
    return 0;
  }

  return app.exec();
}

