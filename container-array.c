/**************************************************************************/
/*                                                                        */
/*  Copyright (C) INTERSEC SA                                             */
/*                                                                        */
/*  Should you receive a copy of this source code, you must check you     */
/*  have a proper, written authorization of INTERSEC to hold it. If you   */
/*  don't have such an authorization, you must DELETE all source code     */
/*  files in your possession, and inform INTERSEC of the fact you obtain  */
/*  these files. Should you not comply to these terms, you can be         */
/*  prosecuted in the extent permitted by applicable law.                 */
/*                                                                        */
/**************************************************************************/

#include "container-array.h"

/**************************************************************************/
/* specific iovec functions                                               */
/**************************************************************************/

int iovec_vector_kill_first(iovec_vector *iovs, ssize_t len)
{
    int i = 0;

    while (i < iovs->len && len >= (ssize_t)iovs->tab[i].iov_len) {
        len -= iovs->tab[i++].iov_len;
    }
    iovec_vector_splice(iovs, 0, i, NULL, 0);
    if (iovs->len > 0 && len) {
        iovs->tab[0].iov_base = (byte *)iovs->tab[0].iov_base + len;
        iovs->tab[0].iov_len  -= len;
    }
    return i;
}

/**************************************************************************/
/* Misc                                                                   */
/**************************************************************************/

static void vector_destroy_skip(generic_vector *v, int el_siz)
{
    memmove((byte *)v->tab - v->skip * el_siz, v->tab, v->len * el_siz);
    v->tab   = (byte *)v->tab - (v->skip * el_siz);
    v->size += v->skip;
    v->skip  = 0;
}

void generic_vector_ensure(generic_vector *v, int newlen, int el_siz)
{
    int newsz;

    if (newlen < 0 || newlen * el_siz < 0)
        e_panic("trying to allocate insane amount of RAM");

    if (newlen <= v->size)
        return;

    /* if data fits and skip is worth it, shift it left */
    if (newlen <= v->skip + v->size && v->skip > v->size / 4) {
        vector_destroy_skip(v, el_siz);
        return;
    }

    /* most of our pool have expensive reallocs wrt a typical memcpy,
     * and optimize the last realloc so we don't want to alloc and free
     */
    if (v->mem_pool != MEM_LIBC) {
        vector_destroy_skip(v, el_siz);
        if (newlen <= v->size)
            return;
    }

    newsz = p_alloc_nr(v->size + v->skip);
    if (newsz < newlen)
        newsz = newlen;
    if (v->mem_pool == MEM_STATIC || v->skip) {
        char *new_area = p_new_raw(char, newsz * el_siz);

        memcpy(new_area, v->tab, v->len * el_siz);
        if (v->mem_pool != MEM_STATIC) {
            /* XXX: v->skip != 0 implies mem_pool == MEM_LIBC */
            libc_free((char *)v->tab - v->skip * el_siz, 0);
        }
        v->tab      = new_area;
        v->mem_pool = MEM_LIBC;
        v->skip     = 0;
        v->size     = newsz;
    } else {
        v->tab = irealloc(v->tab, v->len * el_siz, newsz * el_siz,
                          v->mem_pool | MEM_RAW);
        v->size = newsz;
    }
}

void *generic_array_take(generic_array *array, int pos)
{
    void *ptr;

    if (pos >= array->len || pos < 0) {
        return NULL;
    }

    ptr = array->tab[pos];
    p_move2(array->tab, pos, pos + 1, array->len - pos - 1);
    array->len--;

    return ptr;
}

/**************************************************************************/
/* specific string array functions                                        */
/**************************************************************************/

/* OG: API discussion:
 * str_explode() as a reference to PHP's explode() function but with
 * different semantics: order of parameters is reversed, and delimiter
 * is en actual string, not a set a characters.
 * We could also name this str_split() as a reference to Javascript's
 * String split method.
 * - a NULL token string could mean a standard set of delimiters
 * - an empty token string will produce a singleton
 * - an empty string will produce a NULL instead of an empty array ?
 */
string_array *str_explode(const char *s, const char *tokens)
{
    const char *p;
    string_array *res;

    if (!s || !tokens || !*s) {
        return NULL;
    }

    res = string_array_new();
    p = strpbrk(s, tokens);

    while (p != NULL) {
        string_array_append(res, p_dupz(s, p - s));
        s = p + 1;
        p = strchr(s, *p);
    }

    /* Last part */
    if (*s) {
        string_array_append(res, p_strdup(s));
    }
    return res;
}

/*---------------- Optimized Array Merge Sort ----------------*/

#define comp(a, b, parm)  (*comp)(*(a), *(b), parm)

static void copyvp(void **to, void **from, size_t n)
{
    while (n--) {
        *to++ = *from++;
    }
}

//#define copychunk(d,s,c) memcpy(d,s,(c) * sizeof(void*))
#define copychunk(d,s,c) copyvp(d,s,c)

/* swap 2 arrays of pointers a[0..n] and b[0..n] */
static void vecswap(void **a, void **b, int n)
{
    if (n > 0) {
        size_t i = (size_t)(n);
        register void **pi = a;
        register void **pj = b;
        do {
            register void *t = *pi;
            *pi++ = *pj;
            *pj++ = t;
        } while (--i > 0);
    }
}

/* Merge 2 subarrays a[0..n1] and a[n1..2*n1] */
static
void pqmerge2(void **a, size_t n1, void **tmp,
              int (*comp)(const void *p1, const void *p2, void *parm),
              void *parm)
{
    size_t i, j, k, n3, n;

    /* Test if arrays are already sorted */
    if (comp(a + n1 - 1, a + n1, parm) <= 0)
        return;

    n = n1 + n1;
    /* Test if arrays should be transposed */
    if (comp(a, a + n - 1, parm) > 0) {
        vecswap(a, a + n1, n1);
        return;
    }

    for (i = k = 0, j = n1; i < n1 - 1; i++) {
        if (comp(a + i, a + j, parm) > 0)
            break;
    }
    n3 = n1 - i;
    copychunk(tmp, a + i, n3);
    for (;;) {
        a[i] = a[j];
        i++;
        j++;
        if (j >= n) {
            copychunk(a + i, tmp + k, n3 - k);
            return;
        }
        while (comp(tmp + k, a + j, parm) <= 0) {
            a[i] = tmp[k];
            i++;
            k++;
            if (k >= n3) {
                return;
            }
        }
    }
}

/* find insertion point with binary search, insert key at p2 after all
 * identical keys from a[0..n1]
 */
static
int pqfind(void **a, size_t n, void *key,
           int (*comp)(const void *p1, const void *p2, void *parm),
           void *parm)
{
    size_t i, j, k;

    for (i = 0, j = n; i < j; ) {
        k = (i + j) >> 1;
        if ((*comp)(a[k], key, parm) > 0)
            j = k;
        else
            i = k + 1;
    }
    return i;
}

/* Merge 2 subarrays a[0..n1] and a[n1..n1+n2], n2 <= n1 */
static
void pqmerge(void **a, size_t n1, size_t n2, void **tmp,
             int (*comp)(const void *p1, const void *p2, void *parm),
             void *parm)
{
    size_t i, j, k, n3, n;

    /* Test if arrays are already sorted (1) */
    if (comp(a + n1 - 1, a + n1, parm) <= 0)
        return;

    n = n1 + n2;
    /* Test if arrays should be transposed */
    if (comp(a, a + n - 1, parm) > 0) {
        copychunk(tmp, a, n1);
        copychunk(a, a + n1, n2);
        copychunk(a + n2, tmp, n1);
        return;
    }

    /* Check if subarray n2 is much smaller than n1 */
    i = (n1 + n2) / n2;
    if (i > 30 || (1U << i) > n1 + 1) {
        /* If log2(n1)+1 < (n1+n2)/n2, we can insert n2 elements into the
         * array a[0..n1] in less than n1+n2 comparisons, using binary
         * searches to find merge spots.
         */
        j = n1;
        i = pqfind(a, n1, a[j], comp, parm);
        /* because of (1), we know that i < n1 */
        n3 = n1 - i;
        k = 0;
        copychunk(tmp, a + i, n3);
        for (;;) {
            size_t kk;

            a[i] = a[j];
            i++;
            j++;
            if (j >= n) {
                copychunk(a + i, tmp + k, n3 - k);
                return;
            }
            kk = pqfind(tmp + k, n3 - k, a[j], comp, parm);
            copychunk(a + i, tmp + k, kk);
            i += kk;
            k += kk;
            if (k >= n3)
                return;
        }
    } else {
        /* Use regular array merge loop */
        for (i = k = 0, j = n1; i < n1 - 1; i++) {
            if (comp(a + i, a + j, parm) > 0)
                break;
        }
        n3 = n1 - i;
        copychunk(tmp, a + i, n3);
        for (;;) {
            a[i] = a[j];
            i++;
            j++;
            if (j >= n) {
                copychunk(a + i, tmp + k, n3 - k);
                return;
            }
            while (comp(tmp + k, a + j, parm) <= 0) {
                a[i] = tmp[k];
                i++;
                k++;
                if (k >= n3) {
                    return;
                }
            }
        }
    }
}

static
void pqsort(void *base[], size_t n,
            int (*comp)(const void *p1, const void *p2, void *parm),
            void *parm)
{
#ifdef OS_WINDOWS
    void *tmp_buffer[BUFSIZ / sizeof(void*)];
#endif
    void **tmp;
    size_t step, pos, n2;

    if (n <= 1)
        return;

    /* compute largest power of 2 less than n */
    for (n2 = n - 1; n2 & (n2 - 1); n2 &= n2 - 1)
        continue;

    /* allocate temporary array preferably on the stack */
    if (n2 > BUFSIZ / sizeof(void*)) {
        tmp = p_new(void *, n2);
    } else {
#ifdef OS_WINDOWS
        tmp = tmp_buffer;
#else
        tmp = p_alloca(void *, n2);
#endif
    }

    for (pos = 1; pos < n; pos += 2) {
        if (comp(base + pos - 1, base + pos, parm) > 0) {
            SWAP(void *, base[pos - 1], base[pos]);
        }
        /* Bottom up merge binary subtree */
        for (step = 2; pos & step; step += step) {
            pqmerge2(base + pos + 1 - step - step, step, tmp, comp, parm);
        }
    }
    /* Bottom up merge uneven parts */
    for (pos = n, step = 1; step < n; step += step) {
        pos &= ~step;
        if (pos + step < n) {
            pqmerge(base + pos, step, n - (pos + step), tmp, comp, parm);
        }
    }
    /* free temporary array if allocated from the heap */
    if (n2 > BUFSIZ / sizeof(void*)) {
        p_delete(&tmp);
    }
}

void generic_array_sort(generic_array *array,
                        int (*comp)(const void *p1, const void *p2, void *parm),
                        void *parm)
{
    pqsort(array->tab, array->len, comp, parm);
}

/*[ CHECK ]::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::{{{*/
#ifdef CHECK
/* {{{*/
#include <check.h>

START_TEST(check_str_explode)
{
    string_array *arr;

#define STR_EXPL_CORRECT_TEST(str1, str2, str3, sep)                       \
    arr = str_explode(str1 sep str2 sep str3, sep);                        \
                                                                           \
    fail_if(arr == NULL, "str_explode failed, res == NULL");               \
    fail_if(arr->len != 3, "str_explode failed: len = %d != 3", arr->len); \
    fail_if(strcmp(arr->tab[0], str1),                                     \
            "str_explode failed: tab[0] (%s) != %s",                       \
            arr->tab[0], str1);                                            \
    fail_if(strcmp(arr->tab[1], str2),                                     \
            "str_explode failed: tab[1] (%s) != %s",                       \
            arr->tab[1], str2);                                            \
    fail_if(strcmp(arr->tab[2], str3),                                     \
            "str_explode failed: tab[2] (%s) != %s",                       \
            arr->tab[2], str3);                                            \
                                                                           \
    string_array_delete(&arr);
    STR_EXPL_CORRECT_TEST("123", "abc", "!%*", "/");
    STR_EXPL_CORRECT_TEST("123", "abc", "!%*", " ");
    STR_EXPL_CORRECT_TEST("123", "abc", "!%*", "$");
    STR_EXPL_CORRECT_TEST("   ", ":::", "!!!", ",");

    arr = str_explode("secret1 secret2 secret3", " ,;");
    fail_if(arr == NULL, "str_explode failed, res == NULL");
    fail_if(arr->len != 3, "str_explode failed: len = %d != 3", arr->len);
    string_array_delete(&arr);
    arr = str_explode("secret1;secret2;secret3", " ,;");
    fail_if(arr == NULL, "str_explode failed, res == NULL");
    fail_if(arr->len != 3, "str_explode failed: len = %d != 3", arr->len);
    string_array_delete(&arr);
    arr = str_explode("secret1,secret2 secret3", " ,;");
    fail_if(arr == NULL, "str_explode failed, res == NULL");
    fail_if(arr->len != 2, "str_explode failed: len = %d != 2", arr->len);
    string_array_delete(&arr);
}
END_TEST

Suite *check_str_array_suite(void)
{
    Suite *s  = suite_create("str_array");
    TCase *tc = tcase_create("Core");

    suite_add_tcase(s, tc);
    tcase_add_test(tc, check_str_explode);
    return s;
}
#endif
