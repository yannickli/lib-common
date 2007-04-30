/**************************************************************************/
/*                                                                        */
/*  Copyright (C) 2004-2007 INTERSEC SAS                                  */
/*                                                                        */
/*  Should you receive a copy of this source code, you must check you     */
/*  have a proper, written authorization of INTERSEC to hold it. If you   */
/*  don't have such an authorization, you must DELETE all source code     */
/*  files in your possession, and inform INTERSEC of the fact you obtain  */
/*  these files. Should you not comply to these terms, you can be         */
/*  prosecuted in the extent permitted by applicable law.                 */
/*                                                                        */
/**************************************************************************/

#include "macros.h"
#include "string_is.h"
#include "array.h"

#define ARRAY_INITIAL_SIZE 32

/**************************************************************************/
/* Private inlines                                                        */
/**************************************************************************/

static inline void
array_resize(generic_array *a, ssize_t newlen)
{
    ssize_t curlen = a->len;

    /* Reallocate array if needed */
    if (newlen > a->size) {
        /* OG: Should use p_realloc */
        /* FIXME: should increase array size more at a time:
         * expand by half the current size?
         */
        a->size = MEM_ALIGN(newlen);
        a->tab = (void **)mem_realloc(a->tab, a->size * sizeof(void*));
    }
    /* Initialize new elements to NULL */
    while (curlen < newlen) {
        a->tab[curlen++] = NULL;
    }
    a->len = newlen;
}


/**************************************************************************/
/* Memory management                                                      */
/**************************************************************************/

generic_array *generic_array_init(generic_array *array)
{
    array->tab  = p_new(void *, ARRAY_INITIAL_SIZE);
    array->len  = 0;
    array->size = ARRAY_INITIAL_SIZE;

    return array;
}

void generic_array_wipe(generic_array *array, array_item_dtor_f *dtor)
{
    if (array) {
        if (dtor) {
            ssize_t i;

            for (i = 0; i < array->len; i++) {
                (*dtor)(&array->tab[i]);
            }
        }
        p_delete(&array->tab);
    }
}

void generic_array_delete(generic_array **array, array_item_dtor_f *dtor)
{
    if (*array) {
        generic_array_wipe(*array, dtor);
        p_delete(&*array);
    }
}

/**************************************************************************/
/* Misc                                                                   */
/**************************************************************************/

void *generic_array_take(generic_array *array, ssize_t pos)
{
    void *ptr;

    if (pos >= array->len || pos < 0) {
        return NULL;
    }

    ptr = array->tab[pos];
    memmove(array->tab + pos, array->tab + pos + 1,
            (array->len - pos - 1) * sizeof(void*));
    array->len--;

    return ptr;
}

/* insert item at pos `pos',
   pos interpreted as array->len if pos > array->len */
void generic_array_insert(generic_array *array, ssize_t pos, void *item)
{
    ssize_t curlen = array->len;

    array_resize(array, curlen + 1);

    if (pos < curlen) {
        if (pos < 0) {
            pos = 0;
        }
        memmove(array->tab + pos + 1, array->tab + pos,
                (curlen - pos) * sizeof(void *));
    } else {
        pos = curlen;
    }

    array->tab[pos] = item;
}

/*---------------- Optimized Array Merge Sort ----------------*/

#define swap(a, b)     do { void *t = *(a); *(a) = *(b); *(b) = t; } while (0)
#define comp(a, b, parm)  (*comp)(*(a), *(b), parm)

#define STATIC_INLINE(p) static inline p

STATIC_INLINE(void copyvp(void **to, void **from, size_t n)) {
    while (n--) {
        *to++ = *from++;
    }
}

//#define copychunk(d,s,c) memcpy(d,s,(c) * sizeof(void*))
#define copychunk(d,s,c) copyvp(d,s,c)

/* swap 2 arrays of pointers a[0..n] and b[0..n] */
STATIC_INLINE(void vecswap(void **a, void **b, int n)) {
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
STATIC_INLINE(
void pqmerge2(void **a, size_t n1, void **tmp,
              int (*comp)(const void *p1, const void *p2, void *parm),
              void *parm))
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
STATIC_INLINE(
int pqfind(void **a, size_t n, void *key,
           int (*comp)(const void *p1, const void *p2, void *parm),
           void *parm))
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
STATIC_INLINE(
void pqmerge(void **a, size_t n1, size_t n2, void **tmp,
             int (*comp)(const void *p1, const void *p2, void *parm),
             void *parm))
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

STATIC_INLINE(
void pqsort(void *base[], size_t n,
            int (*comp)(const void *p1, const void *p2, void *parm),
            void *parm))
{
    void **tmp;
    size_t step, pos, n2;

    if (n <= 1)
        return;

    /* compute largest power of 2 less than n */
    for (n2 = n - 1; n2 & (n2 - 1); n2 &= n2 - 1)
        continue;

    /* allocate temporary array preferably on the stack */
    if (n2 > BUFSIZ / sizeof(void*)) {
        tmp = malloc(n2 * sizeof(void*));
    } else {
        tmp = alloca(n2 * sizeof(void*));
    }
    
    for (pos = 1; pos < n; pos += 2) {
        if (comp(base + pos - 1, base + pos, parm) > 0) {
            swap(base + pos - 1, base + pos);
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
        free(tmp);
    }
}

void generic_array_sort(generic_array *array,
                        int (*comp)(const void *p1, const void *p2, void *parm),
                        void *parm)
{
    pqsort(array->tab, array->len, comp, parm);
}
