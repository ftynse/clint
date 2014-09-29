#ifndef MACROS_H
#define MACROS_H

#include <iostream>

#define CLINT_ASSERT(condition, text) \
  if (!(condition)) { _clint_assert_(#condition, text, __FILE__, __LINE__); }

#define CLINT_UNREACHABLE \
  _clint_assert_("false", "Unrechable code", __FILE__, __LINE__);

__attribute__((noreturn)) inline void _clint_assert_(const char *condition, const char *text, const char *filename, unsigned line) {
  std::cerr << "ASSERTION FAILED: " << text << " (" << condition << ") at " << filename << ":" << line << std::endl;
#ifndef CLINT_NOFATAL_ASSERTS
  std::abort();
#endif
}

inline void _clint_fatal_(const char *str) {
  std::cerr << "FATAL: " << str << std::endl;
  std::abort();
}

inline void _clint_warning_(const char *str) {
  std::cerr << "WARNING: " << str << std::endl;
}

#endif // MACROS_H
