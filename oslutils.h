#ifndef OSLUTILS_H
#define OSLUTILS_H

#include <osl/osl.h>

#include <clay/beta.h>

#include <functional>
#include <map>
#include <tuple>
#include <utility>

/// OpenScop linked-list foreach
#define LL_FOREACH(var, container) \
  for (var = container; var != nullptr; var = var->next)

template <typename T, typename Func, typename... Args>
T *oslListTransform(T *container, Func f, Args... args) {
  T *result = nullptr, *ptr = nullptr;
  if (container == nullptr) {
    return nullptr;
  }
  result = f(container, args...);
  ptr = result;
  for (T *container_part = container->next; container_part != nullptr; container_part = container_part->next) {
    T *result_part = f(container_part, args...);
    ptr->next = result_part;
    ptr = ptr->next;
  }
  return result;
}

/// Call a function on an OSL object as if this object was not part of the list.
/// Function f takes an OSL object of intereset as the first argument, other arguments are optional.
template <typename T, typename Func, typename... Args,
          typename std::enable_if<std::is_void<typename std::result_of<Func(T *, Args...)>::type>::value>::type * = nullptr>
inline auto oslListNoSeqCall(T *ptr, Func f, Args... args) -> decltype(f(ptr, args...)) {
  T *keeper = ptr->next;
  ptr->next = nullptr;
  f(ptr, args...);
  ptr->next = keeper;
}

/// Call a function returning a value for every part of the linked-list OSL object.
/// Function f takes an OSL object of intereset as the first argument, other arguments are optional.
/// If function returns a value, it is ignored.
/// \see oslListTransform to keep the values returned by the function and prevent possible memory leaks.
template <typename T, typename Func, typename... Args,
          typename std::enable_if<!std::is_void<typename std::result_of<Func(T *, Args...)>::type>::value>::type * = nullptr>
inline auto oslListNoSeqCall(T *ptr, Func f, Args... args) -> decltype(f(ptr, args...)) {
  T *keeper = ptr->next;
  ptr->next = nullptr;
  auto result = f(ptr, args...);
  ptr->next = keeper;
  if (std::is_move_assignable<decltype(f(ptr, args...))>::value) {
    return std::move(result);
  } else {
    return result;
  }
}

/// Call a function returning void for every part of the linked-list OSL object.
/// Function f takes an OSL object of intereset as the first argument, other arguments are optional.
/// If function returns a value, it is ignored.
/// \see oslListTransform to keep the values returned by the function and prevent possible memory leaks.
template <typename T, typename Func, typename... Args>
inline void oslListForeach(T *container, Func f, Args... args) {
  for (T *container_part = container; container_part != nullptr; container_part = container_part->next) {
    f(container_part, args...);
  }
}

template <typename T, typename Func, typename... Args>
void oslListForeachSingle(T *container, Func f, Args... args) {
  auto function = [&](T *ptr, Args... a) -> typename std::result_of<Func>::type {
    oslListNoSeqCall(ptr, f, a...);
  };
  oslListForeach(container, function, args...);
}

template <typename T>
T *oslListPrev(T *var, T *container) {
  T *t;
  for (t = container; t != nullptr; t = t->next) {
    if (t->next == var) {
      break;
    }
  }
  return t;
}

template <typename T>
std::vector<T *> oslListToVector(T *container) {
  std::vector<T *> vector;
  for (T *ptr = container; ptr != nullptr; ptr = ptr->next) {
    vector.push_back(ptr);
  }
  return std::move(vector);
}

template <typename T>
T *oslListFromVector(const std::vector<T *> &vector) {
  if (vector.size() == 0)
    return nullptr;
  T *head = vector.front();
  T *ptr  = head;
  for (size_t i = 1; i < vector.size(); i++) {
    ptr->next = vector[i];
    ptr = ptr->next;
  }
}


osl_relation_p oslApplyScattering(osl_statement_p stmt);
osl_relation_p oslApplyScattering(osl_statement_p stmt, const std::vector<int> &beta);
osl_relation_p oslApplyScattering(const std::vector<osl_relation_p> &domains,
                                  const std::vector<osl_relation_p> &scatterings);

osl_relation_p oslRelationWithContext(osl_relation_p relation, osl_relation_p context);

osl_relation_p oslRelationFixParameters(osl_relation_p relation, const std::vector<std::pair<bool, int>> &values);
osl_relation_p oslRelationFixAllParameters(osl_relation_p relation, int value);

osl_scop_p prepareEnumeration(osl_scop_p scop);

inline osl_relation_p oslRelationsFixParameters(osl_relation_p relation, const std::vector<std::pair<bool, int>> &values) {
  return oslListTransform(relation, &oslRelationFixParameters, values);
}

inline osl_relation_p oslRelationsFixAllParameters(osl_relation_p relation, int value) {
  return oslListTransform(relation, &oslRelationFixAllParameters, value);
}

inline osl_relation_p oslRelationsWithContext(osl_relation_p relation, osl_relation_p context) {
  return oslListTransform(relation, &oslRelationWithContext, context);
}

// Vectors are lexicographically-comparable.
typedef std::map<std::vector<int>,
                 std::tuple<osl_scop_p, osl_statement_p, osl_relation_p>
                > BetaMap;

BetaMap oslBetaMap(osl_scop_p scop);

// FIXME: we use Clay to access betas; when this functionality is moved to OSL, switch!
std::vector<int> betaFromClay(clay_array_p beta);
clay_array_p clayBetaFromVector(const std::vector<int> &betaVector);

inline std::vector<int> betaExtract(osl_relation_p relation) {
  clay_array_p clay_beta = clay_beta_extract(relation);
  std::vector<int> beta = betaFromClay(clay_beta);
  clay_array_free(clay_beta);
  return std::move(beta);
}

#endif // OSLUTILS_H
