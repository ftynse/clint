#ifndef MACROS_H
#define MACROS_H

#include <iostream>

#define CLINT_WARNING_WARNING  1
#define CLINT_WARNING_INFO     2
#define CLINT_WARNING_DEBUG    3

#define CLINT_WARNING_LEVEL  CLINT_WARNING_DEBUG

#define CLINT_ASSERT(condition, text) \
  do { \
    if (!(condition)) { _clint_assert_(#condition, text, __FILE__, __LINE__); } \
  } while (false) \

#define CLINT_WARNING(condition, text) \
  CLINT_LEVELED_MESSAGE(condition, text, CLINT_WARNING_WARNING)

#define CLINT_INFO(condition, text) \
  CLINT_LEVELED_MESSAGE(condition, text, CLINT_WARNING_INFO)

#define CLINT_DEBUG(condition, text) \
  CLINT_LEVELD_MESSAGE(condition, text, CLINT_WARNING_DEBUG)

#define CLINT_LEVELED_MESSAGE(condition, text, level) \
  do { \
   if (!(condition)) {_clint_warning_("condition failed" #condition, text, __FILE__, __LINE__, level); } \
  } while (false) \

#define CLINT_UNREACHABLE  \
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

inline void _clint_warning_(const char *condition, const char *text, const char *filename, unsigned line, int level) {
#ifdef CLINT_WARNING_LEVEL
  if (CLINT_WARNING_LEVEL >= level) {
    std::cerr << "[Clint] W" << level << ": " << text << " (" << condition << ") at " << filename << ":" << line << std::endl;
  }
#endif
}

#endif // MACROS_H
