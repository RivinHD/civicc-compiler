#pragma once

#ifdef __APPLE__

#include <_assert.h>
#define release_assert(expr) ((expr) ? (void)0 : __assert(#expr, __FILE__, __LINE__))

#else // Linux

#include <sys/cdefs.h>
__BEGIN_DECLS

/* Declaration extracted from assert.h*/
/* This prints an "Assertion failed" message and aborts.  */
#ifndef __COLD // For older version __COLD is not defined so we ignore it.
#define __COLD
#endif // !__COLD
extern void __assert_fail(const char *__assertion, const char *__file, unsigned int __line,
                          const char *__function) __THROW __attribute__((__noreturn__)) __COLD;

__END_DECLS

#define release_assert(expr)                                                                       \
    ((expr) ? (void)0 : __assert_fail(#expr, __FILE__, __LINE__, __extension__ __PRETTY_FUNCTION__))

#endif
