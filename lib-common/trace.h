#ifndef TRACE_H
#define TRACE_H
#include <lib-common/macros.h>
void trace(int level, const char *fmt, ...) __attr_format__(2, 3);
void trace_increase(void);
#endif
