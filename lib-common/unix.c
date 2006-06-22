#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include "unix.h"

/* Returns 0 if directory exists,
 * Returns 1 if directory was created
 * Returns -1 in case an error occured.
 */
int mkdir_p(const char *dir, mode_t mode)
{
    int ret = 0;
    char *p;
    char *dir2;
    struct stat buf;

    dir2 = strdup(dir);
    if (!dir2) {
        ret = -1;
        goto end;
    }

    /* Creating "/a/b/c/d" where "/a/b" exists but not "/a/b/c".
     * First find that "/a/b" exists, replacing slashes by \0 (sse below)
     */
    while (stat(dir2, &buf) != 0) {
        if (errno != ENOENT) {
            ret = -1;
            goto end;
        }
        ret = 1;
        p = strrchr(dir2, '/');
        if (p == NULL) {
            break;
        }
        *p = '\0';
    }
    if (!S_ISDIR(buf.st_mode)) {
        ret = -1;
        goto end;
    }
    if (ret == 0) {
        goto end;
    }

    /* Then, create /a/b/c and /a/b/c/d : we just have to put '/' where
     * we put \0 in the previous loop. */
    for(;;) {
        p = dir2 + strlen(dir2);
        *p = '/';
        if (mkdir(dir2, mode) != 0) {
            /* if dir = "/a/../b", then we do a mkdir("/a/..") => EEXIST,
             * but we want to continue walking the path to create /b !
             */
            if (errno != EEXIST) {
                free(dir2);
                return 1;
            }
        }
        if (!strcmp(dir, dir2)) {
            break;
        }
    }

end:
    free(dir2);
    return ret;
}
