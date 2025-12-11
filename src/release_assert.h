#pragma once

#include <sys/cdefs.h>

#if __APPLE__
#include <_assert.h>
#define release_assert(expr)                                                                       \
    (__builtin_expect(!(expr), 0) ? __assert_rtn(__func__, __ASSERT_FILE_NAME, __LINE__, #expr)    \
                                  : (void)0)
#else
__BEGIN_DECLS

/* Declaration extracted from assert.h*/
/* This prints an "Assertion failed" message and aborts.  */
extern void __assert_fail(const char *__assertion, const char *__file, unsigned int __line,
                          const char *__function) __THROW __attribute__((__noreturn__)) __COLD;

__END_DECLS

#define release_assert(expr)                                                                       \
    ((expr) ? (void)0 : __assert_fail(#expr, __FILE__, __LINE__, __extension__ __PRETTY_FUNCTION__))
#endif
