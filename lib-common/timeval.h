#ifndef IS_TIMEVAL_H
#define IS_TIMEVAL_H
#include <sys/time.h>
#include <time.h>

#include <lib-common/macros.h>

#ifndef NDEBUG
/* Return reference to static buf for immediate printing */
const char *timeval_format(struct timeval tv);
#endif

struct timeval timeval_add(struct timeval a, struct timeval b);
struct timeval timeval_sub(struct timeval a, struct timeval b);
bool is_expired(const struct timeval *date, const struct timeval *now,
                struct timeval *left);
#endif
