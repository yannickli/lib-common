#include <string.h>

#include "err_report.h"
#include "string_is.h"

ssize_t sstrlen(const unsigned char * str)
{
    ssize_t res = (ssize_t)strlen((const char *)str);
    if (res < 0) {
        e_fatal(FATAL_NOMEM,
                E_PREFIX("strings greater than MAX_SSIZE_T-1 octets are not allowed"));
    }
    return res;
}

