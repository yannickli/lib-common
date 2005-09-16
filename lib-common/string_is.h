#ifndef IS_STRING_IS_H
#define IS_STRING_IS_H

#include <stdlib.h>
#include <string.h>

static inline ssize_t sstrlen(const char * str)
{
    return (ssize_t)strlen((const char *)str);
}

#endif
