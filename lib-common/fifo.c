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
#include "fifo.h"

#define FIFO_INITIAL_SIZE 32

static inline void fifo_grow(fifo *f, ssize_t newsize)
{
    ssize_t cursize = f->size;

    if (newsize < cursize) {
        return;
    }
    /* OG: Should use p_realloc */
    newsize = MEM_ALIGN(newsize);
    f->size = newsize;
    p_realloc(&f->elems, newsize);
    if (f->first + f->nb_elems > cursize) {
        ssize_t firstpartlen, secondpartlen;

        /* elements are split in two parts. Move the shortest one */
        secondpartlen = cursize - f->first;
        firstpartlen = f->nb_elems - secondpartlen;
        if (firstpartlen > secondpartlen
        ||  firstpartlen > (newsize - cursize))
        {
            memmove(&f->elems[newsize - secondpartlen],
                    &f->elems[f->first],
                    secondpartlen * sizeof(void *));
            f->first = newsize - secondpartlen;
        } else {
            memcpy(&f->elems[cursize],
                   &f->elems[0], firstpartlen * sizeof(void*));
        }
    }
}

fifo *fifo_init_nb(fifo *f, ssize_t size)
{
    f->elems = p_new(void *, size);
    f->nb_elems = 0;
    f->first = 0;
    f->size = size;

    return f;
}

fifo *fifo_init(fifo *f)
{
    return fifo_init_nb(f, FIFO_INITIAL_SIZE);
}

void fifo_wipe(fifo *f, fifo_item_dtor_f *dtor)
{
    if (f) {
        if (dtor) {
            ssize_t i;
            ssize_t last;

            last = f->first + f->nb_elems;
            if (last < f->size) {
                for (i = f->first; i < last; i++) {
                    (*dtor)(&f->elems[i]);
                }
            } else {
                for (i = f->first; i < f->size; i++) {
                    (*dtor)(&f->elems[i]);
                }
                last = f->nb_elems - (f->size - f->first);
                for (i = 0; i < last; i++) {
                    (*dtor)(&f->elems[i]);
                }
            }
        }
        p_delete(&f->elems);
        f->first = 0;
        f->size = 0;
        f->nb_elems = 0;
    }
}

void fifo_delete(fifo **f, fifo_item_dtor_f *dtor)
{
    if (*f) {
        fifo_wipe(*f, dtor);
        p_delete(&*f);
    }
}

void *fifo_get(fifo *f)
{
    void *ptr;

    if (f->nb_elems <= 0) {
        return NULL;
    }

    ptr = f->elems[f->first];
    f->elems[f->first] = NULL;
    f->nb_elems--;

    if (f->nb_elems == 0) {
        return ptr;
    }

    f->first++;
    if (f->first == f->size) {
        f->first = 0;
    }

    return ptr;
}

void fifo_put(fifo *f, void *ptr)
{
    ssize_t cur;

    if (f->size == f->nb_elems) {
        fifo_grow(f, f->size * 2);
    }
    
    cur = f->first + f->nb_elems;
    if (cur >= f->size) {
        cur -= f->size;
    }

    f->elems[cur] = ptr;
    f->nb_elems++;
}

#ifdef CHECK
#include <stdio.h>
static void fifo_dump(fifo *f)
{
    int i;

    printf("nb_elems:%zd first:%zd [", f->nb_elems, f->first);

    for (i = 0; i < f->first; i++) {
        printf("%p ", f->elems[i]);
    }
    printf("\n");
}

static void fifo_nop(void *f)
{
    f = f;
}

START_TEST(check_init_wipe)
{
    fifo f;
    fifo_init(&f);
    fail_if(f.size != FIFO_INITIAL_SIZE);
    fifo_wipe(&f, fifo_nop);
}
END_TEST

START_TEST(check_get_empty)
{
    fifo f;
    fifo_init(&f);
    fail_if(fifo_get(&f) != NULL);
    fifo_wipe(&f, NULL);
}
END_TEST

#define i __i
START_TEST(check_smallrun)
#undef i
{
    int i;
    int tab[10];
    fifo f;

    for (i = 0; i < 10; i++) {
        tab[i] = i;
    }

    fifo_init(&f);

    fifo_put(&f, &tab[0]);
    fifo_put(&f, &tab[1]);
    fifo_put(&f, &tab[2]);
    fifo_put(&f, &tab[3]);
    fail_if(fifo_get(&f) != &tab[0]);
    fail_if(fifo_get(&f) != &tab[1]);
    fail_if(fifo_get(&f) != &tab[2]);
    fail_if(fifo_get(&f) != &tab[3]);
    fail_if(fifo_get(&f) != NULL);

    fifo_wipe(&f, NULL);
}
END_TEST

#define i __i
START_TEST(check_roll)
#undef i
{
    int i, j;
    int tab[10];
    fifo f;

    for (i = 0; i < 10; i++) {
        tab[i] = i;
    }

    fifo_init_nb(&f, 5);

    for (j = 0; j < 100; j++) {
        for (i = 0; i < 4; i++) {
            // fifo_dump(&f);
            fifo_put(&f, &tab[i%5]);
        }

        for (i = 0; i < 4; i++) {
            // fifo_dump(&f);
            fail_if(fifo_get(&f) != &tab[i%5], "i=%d j=%d \n", i, j);
        }

        fail_if(fifo_get(&f) != NULL);
    }

    fifo_wipe(&f, NULL);
}
END_TEST

#define i __i
START_TEST(check_grow)
#undef i
{
    int i;
    int tab[10];
    fifo f;

    for (i = 0; i < 10; i++) {
        tab[i] = i;
    }

    fifo_init(&f);

    for (i = 0; i < 1500; i++) {
        fifo_put(&f, &tab[i%3]);
    }

    for (i = 0; i < 1500; i++) {
        fail_if(fifo_get(&f) != &tab[i%3], "i=%d\n", i);
    }

    fail_if(fifo_get(&f) != NULL);

    fifo_wipe(&f, NULL);
}
END_TEST

Suite *check_fifo_suite(void)
{
    Suite *s  = suite_create("Fifo");
    TCase *tc = tcase_create("Core");

    suite_add_tcase(s, tc);
    tcase_add_test(tc, check_init_wipe);
    tcase_add_test(tc, check_get_empty);
    tcase_add_test(tc, check_smallrun);
    tcase_add_test(tc, check_roll);
    tcase_add_test(tc, check_grow);
    return s;
}
#endif
