/**************************************************************************/
/*                                                                        */
/*  Copyright (C) 2004-2008 INTERSEC SAS                                  */
/*                                                                        */
/*  Should you receive a copy of this source code, you must check you     */
/*  have a proper, written authorization of INTERSEC to hold it. If you   */
/*  don't have such an authorization, you must DELETE all source code     */
/*  files in your possession, and inform INTERSEC of the fact you obtain  */
/*  these files. Should you not comply to these terms, you can be         */
/*  prosecuted in the extent permitted by applicable law.                 */
/*                                                                        */
/**************************************************************************/

#include "fifo.h"

static int fifo_real_pos(fifo *f, int idx)
{
    int pos = f->first + idx;
    return pos >= f->size ? pos - f->size : pos;
}

static void fifo_grow(fifo *f, int newsize)
{
    int cursize = f->size;

    if (newsize <= cursize)
        return;

    p_allocgrow(&f->elems, newsize, &f->size);
    if (f->first + f->nb_elems > cursize) {
        /* elements are split in two parts. Move the shortest one */
        int right_len = cursize - f->first;
        int left_len  = f->nb_elems - right_len;

        if (left_len > right_len || left_len > (f->size - cursize)) {
            p_move(f->elems, f->size - right_len, f->first, right_len);
            f->first = f->size - right_len;
        } else {
            p_copy(f->elems, cursize, 0, left_len);
        }
    }
}

void fifo_wipe(fifo *f, fifo_item_dtor_f *dtor)
{
    if (f) {
        if (dtor) {
            int last = f->first + f->nb_elems;
            if (last < f->size) {
                for (int i = f->first; i < last; i++) {
                    (*dtor)(&f->elems[i]);
                }
            } else {
                for (int i = f->first; i < f->size; i++) {
                    (*dtor)(&f->elems[i]);
                }
                last = f->nb_elems - (f->size - f->first);
                for (int i = 0; i < last; i++) {
                    (*dtor)(&f->elems[i]);
                }
            }
        }
        p_delete(&f->elems);
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
        f->first = 0;
    } else {
        f->first++;
        if (f->first == f->size) {
            f->first = 0;
        }
    }

    return ptr;
}

void fifo_put(fifo *f, void *ptr)
{
    fifo_grow(f, f->nb_elems + 1);
    f->elems[fifo_real_pos(f, f->nb_elems)] = ptr;
    f->nb_elems++;
}

void fifo_unget(fifo *f, void *ptr)
{
    fifo_grow(f, f->nb_elems + 1);
    f->first--;
    if (f->first < 0) {
        f->first += f->size;
    }
    f->elems[f->first] = ptr;
    f->nb_elems++;
}

void *fifo_seti(fifo *f, int i, void *ptr)
{
    int pos, last;

    if (i >= f->nb_elems) {
        fifo_grow(f, i + 1);
    }

    pos  = fifo_real_pos(f, i);
    last = fifo_real_pos(f, f->nb_elems);

    if (i < f->nb_elems) {
        SWAP(void *, f->elems[pos], ptr);
        return ptr;
    }

    if (pos > last) {
        p_clear(f->elems + last, pos - last);
    } else {
        p_clear(f->elems + last, f->size - last);
        p_clear(f->elems, pos);
    }

    f->nb_elems = i + 1;
    f->elems[pos] = ptr;
    return NULL;
}

void *fifo_geti(fifo *f, int i)
{
    return i >= f->nb_elems ? NULL : f->elems[fifo_real_pos(f, i)];
}

#ifdef CHECK /* {{{ */
static void fifo_dump(fifo *f)
{
    int i;

    printf("nb_elems:%d first:%d [", f->nb_elems, f->first);

    for (i = 0; i < f->first; i++) {
        printf("%p ", f->elems[i]);
    }
    printf("\n");
}

static void fifo_nop(void *f)
{
    f = f;
}

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

    fifo_init(&f);

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
    tcase_add_test(tc, check_get_empty);
    tcase_add_test(tc, check_smallrun);
    tcase_add_test(tc, check_roll);
    tcase_add_test(tc, check_grow);
    return s;
}
#endif /* }}} */
