#ifndef ENUMERATOR_H
#define ENUMERATOR_H

#include <isl/set.h>
#include <isl/map.h>
#include <isl/ctx.h>

#include <osl/osl.h>

#include "macros.h"

class Enumerator {
public:
  /**
   * @brief Get a list of integer points in the polytope, projected to a set of specific dimensions.
   * @param [in] relation   Union of relations that defines a polytope.
   * @param [in] dimensions Vector of indices of dimensions to project points onto.
   * @return The vector of integer points inside the polyhedron, each point represented as a vector of integer coordinates ordered
   * by the dimension index.
   */
  virtual std::vector<std::vector<int>> enumerate(osl_relation_p relation, const std::vector<int> &dimensions) = 0;
  /**
   * @brief Get the logical execution date of a statement instance identified by the given vector if iterator values.
   * @param [in] stmt  Statement, instance of which is being projected.
   * @param [in] point Iterator values for the statement instance.
   * @return Logical execution date in beta-alpha format.
   */
  virtual std::vector<int> map(osl_statement_p stmt, const std::vector<int> &point) = 0;
  /**
   * @brief Virtual desctructor.  Reimplement in all derived classes with non-trival memory management.
   */
  virtual ~Enumerator() {}
};

class ISLContextRAII {
public:
  ISLContextRAII() {
  }

  ~ISLContextRAII() {
    isl_ctx_free(ctx_);
  }

  isl_ctx *ctx() {
    if (ctx_ == nullptr) {
      ctx_ = isl_ctx_alloc();
    }
    return ctx_;
  }

  isl_ctx *operator ()() {
    return ctx();
  }

  operator isl_ctx *() {
    return ctx();
  }

private:
  isl_ctx *ctx_;
};

/**
 * @brief An implementation of Enumerator using ISL library.
 */
class ISLEnumerator : public Enumerator {
public:
  std::vector<std::vector<int> > enumerate(osl_relation_p relation, const std::vector<int> &dimensions) override;
  std::vector<int> map(osl_statement_p stmt, const std::vector<int> &point) override;

  ~ISLEnumerator() override;

  static std::tuple<isl_dim_type, int> dimFromOSL(osl_relation_p relation, int dimension) {
    CLINT_ASSERT(dimension >= 0, "Negative dimension");
    CLINT_ASSERT(dimension < relation->nb_columns - 2, "Dimension overflow");
    if (dimension < relation->nb_input_dims)
      return std::make_tuple(isl_dim_in, dimension);
    dimension -= relation->nb_input_dims;
    if (dimension < relation->nb_output_dims)
      return std::make_tuple(isl_dim_out, dimension);
    dimension -= relation->nb_output_dims;
    if (dimension < relation->nb_local_dims)
      CLINT_ASSERT(false, "Local dimensions are not directly accessible in ISL");
    dimension -= relation->nb_local_dims;
    if (dimension < relation->nb_parameters)
      return std::make_tuple(isl_dim_param, dimension);

    CLINT_UNREACHABLE;
  }

  static inline __isl_give isl_set *setFromOSLRelation(osl_relation_p relation, osl_names_p names = nullptr) {
    CLINT_ASSERT(relation->nb_input_dims == 0, "Relation is not a set");
    return osl2isl(isl_set_read_from_str, relation, names);
  }

  static inline __isl_give isl_map *mapFromOSLRelation(osl_relation_p relation, osl_names_p names = nullptr) {
    return osl2isl(isl_map_read_from_str, relation, names);
  }

  static inline __isl_give isl_basic_set *basicSetFromOSLRelation(osl_relation_p relation, osl_names_p names = nullptr) {
    CLINT_ASSERT(relation->nb_input_dims == 0, "Relation is not a set");
    return osl2isl(isl_basic_set_read_from_str, relation, names);
  }

  static inline __isl_give isl_basic_map *basicMapFromOSLRelation(osl_relation_p relation, osl_names_p names = nullptr) {
    return osl2isl(isl_basic_map_read_from_str, relation, names);
  }

  static inline osl_relation_p setToOSLRelation(isl_set *set) {
    return isl2osl(isl_printer_print_set, set);
  }

  static inline osl_relation_p basicSetToOSLRelation(isl_basic_set *basicSet) {
    return isl2osl(isl_printer_print_basic_set, basicSet);
  }

  static inline osl_relation_p mapToOSLRelation(isl_map *map) {
    return isl2osl(isl_printer_print_map, map);
  }

  static osl_relation_p scheduledDomain(osl_relation_p domain, osl_relation_p schedule);

private:
  static ISLContextRAII islContext_;

  template <typename T>
  static osl_relation_p isl2osl(isl_printer *(&Func)(isl_printer *, T *), T *t) {
    CLINT_ASSERT(t != nullptr, "ISL object is null");

    isl_printer *prn = isl_printer_to_str(islContext_);
    prn = isl_printer_set_output_format(prn, ISL_FORMAT_EXT_POLYLIB);
    prn = Func(prn, t);
    char *str = isl_printer_get_str(prn);
    char *oslStr = new char[strlen(str) + 11];
    char *oslStrKeeper = oslStr; // osl_relation_sread advances the pointer...
    sprintf(oslStr, "UNDEFINED\n%s", str);
    osl_relation_p relation = osl_relation_sread(&oslStr); // TODO: add osl_relation_sread_polylib to facilitate this
    delete [] oslStrKeeper;
    free(str);
    isl_printer_free(prn);
    return relation;
  }

  template <typename T>
  static T *osl2isl(T *(&Func)(isl_ctx *, const char *), osl_relation_p relation, osl_names_p names = nullptr) {
    CLINT_ASSERT(relation != nullptr, "Relation pointer is null");

    char *string = osl_relation_spprint_polylib(relation, NULL);
    T *t = Func(islContext_, string);
    free(string);
    return t;
  }
};

#endif // ENUMERATOR_H
