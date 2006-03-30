#include <stdarg.h>
#include <stdio.h>
#include <lib-common/trace.h>

static int verbose = 0;

void trace(int level, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    if (verbose >= level) {
        vfprintf(stderr, fmt, ap);
        fflush(stderr);
    }
    va_end(ap);
}

void trace_increase(void)
{
    verbose++;
}

