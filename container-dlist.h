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

#ifndef IS_LIB_COMMON_CONTAINER_DLIST_H
#define IS_LIB_COMMON_CONTAINER_DLIST_H

/* XXX don't include core.h since dlist are used by core-mem-stack, itself
 * used by core.h
 */
#include <stdbool.h>

typedef struct dlist_t {
    struct dlist_t *next, *prev;
} dlist_t;

#define DLIST_INIT(name)  { .next = &(name), .prev = &(name) }
#define DLIST(name)       dlist_t name = DLIST_INIT(name)
static inline void dlist_init(dlist_t *l)
{
    l->next = l->prev = l;
}

/* XXX: WARNING THIS DOESN'T WORK ON A SINGLE NODE OUTSIDE A LIST
 *
 * If this can happen, you have to:
 *
 *   bool was_empty = dlist_is_empty(&ptr->link);
 *   ptr = realloc(ptr, 1209);
 *   if (was_empty) {
 *       dlist_init(&ptr->link);
 *   } else {
 *       __dlist_repair(&ptr->link);
 *   }
 *
 */
static inline void __dlist_repair(dlist_t *e)
{
    /* Use this to repair a dlist_t node in a structure after a realloc() */
    e->next->prev = e;
    e->prev->next = e;
}

static inline void __dlist_add(dlist_t *e, dlist_t *prev, dlist_t *next)
{
    next->prev = e;
    e->next = next;
    e->prev = prev;
    prev->next = e;
}
static inline void
__dlist_remove(dlist_t *prev, dlist_t *next)
{
    next->prev = prev;
    prev->next = next;
}

static inline void dlist_add(dlist_t *l, dlist_t *n) {
    __dlist_add(n, l, l->next);
}
static inline void dlist_add_tail(dlist_t *l, dlist_t *n) {
    __dlist_add(n, l->prev, l);
}
static inline void dlist_add_before(dlist_t *e, dlist_t *n) {
    __dlist_add(n, e->prev, e);
}
static inline void dlist_add_after(dlist_t *e, dlist_t *n) {
    __dlist_add(n, e, e->next);
}

static inline void dlist_remove(dlist_t *l) {
    __dlist_remove(l->prev, l->next);
    dlist_init(l);
}
static inline void dlist_pop(dlist_t *l) {
    dlist_remove(l->next);
}

static inline void dlist_move(dlist_t *l, dlist_t *n) {
    __dlist_remove(n->prev, n->next);
    dlist_add(l, n);
}
static inline void dlist_move_tail(dlist_t *l, dlist_t *n) {
    __dlist_remove(n->prev, n->next);
    dlist_add_tail(l, n);
}

static inline bool
dlist_is_first(const dlist_t *l, const dlist_t *n) {
    return n->prev == l;
}
static inline bool
dlist_is_last(const dlist_t *l, const dlist_t *n) {
    return n->next == l;
}
static inline bool dlist_is_empty(const dlist_t *l) {
    return l->next == l;
}
static inline bool dlist_is_singular(const dlist_t *l) {
    return l->next != l && l->next == l->prev;
}
static inline bool dlist_is_empty_or_singular(const dlist_t *l) {
    return l->next == l->prev;
}


static inline void
__dlist_splice2(dlist_t *prev, dlist_t *next, dlist_t *first, dlist_t *last)
{
    first->prev = prev;
    prev->next  = first;
    last->next  = next;
    next->prev  = last;
}

static inline void
__dlist_splice(dlist_t *prev, dlist_t *next, dlist_t *src)
{
    __dlist_splice2(prev, next, src->next, src->prev);
    dlist_init(src);
}

static inline void
dlist_splice(dlist_t *dst, dlist_t *src)
{
    if (!dlist_is_empty(src)) {
        __dlist_splice(dst, dst->next, src);
    }
}

static inline void
dlist_splice_tail(dlist_t *dst, dlist_t *src)
{
    if (!dlist_is_empty(src)) {
        __dlist_splice(dst->prev, dst, src);
    }
}

static inline void
dlist_cut_at(dlist_t *src, dlist_t *e, dlist_t *dst)
{
    if (dlist_is_empty(src) || src == e) {
        dlist_init(dst);
    } else {
        dlist_t *e_next = e->next;

        dst->next = src->next;
        dst->next->prev = dst;
        dst->prev = e;
        e->next = dst;

        src->next = e_next;
        e_next->prev = src;
    }
}


#define dlist_entry(ptr, type, member)  container_of(ptr, type, member)
#define dlist_entry_of(ptr, n, member)  dlist_entry(ptr, typeof(*n), member)
#define dlist_entry_safe(ptr, type, member) \
    ({ typeof(ptr) *__ptr = (ptr);          \
       __ptr ? dlist_entry(__ptr, type, member) : NULL })
#define dlist_entry_of_safe(ptr, n, member) \
    dlist_entry_safe(ptr, typeof(*n), member)

#define dlist_next_entry(e, mber)  dlist_entry((e)->mber.next, typeof(*e), mber)
#define dlist_prev_entry(e, mber)  dlist_entry((e)->mber.prev, typeof(*e), mber)
#define dlist_first_entry(l, type, mber)  dlist_entry((l)->next, type, mber)
#define dlist_last_entry(l, type, mber)   dlist_entry((l)->prev, type, mber)

#define __dlist_for_each(pos, n, head, doit) \
     for (dlist_t *n = (pos); \
          n != (head) && ({ doit; 1; }); n = n->next)

#define dlist_for_each(n, head) \
    __dlist_for_each((head)->next, n, head, )
#define dlist_for_each_start(pos, n, head) \
    __dlist_for_each(pos, n, head, )
#define dlist_for_each_continue(pos, n, head) \
    __dlist_for_each((pos)->next, n, head, )


#define __dlist_for_each_safe(pos, n, __next, head, doit) \
    for (dlist_t *n = pos, *__next = n->next; \
         n != (head) && ({ doit; 1; }); \
         n = __next, __next = n->next)

#define dlist_for_each_safe(n, head) \
    __dlist_for_each_safe((head)->next, n, __next_##n, head, )
#define dlist_for_each_safe_start(pos, n, head) \
    __dlist_for_each_safe(pos, n, __next_##n, head, )
#define dlist_for_each_safe_continue(pos, n, head) \
    __dlist_for_each_safe((pos)->next, n, __next_##n, head, )


#define __dlist_for_each_rev(pos, n, head, doit) \
     for (dlist_t *n = (pos); \
          n != (head) && ({ doit; 1; }); n = n->prev)

#define dlist_for_each_rev(n, head) \
    __dlist_for_each_rev((head)->prev, n, head, )
#define dlist_for_each_rev_start(pos, n, head) \
    __dlist_for_each_rev(pos, n, head, )
#define dlist_for_each_rev_continue(pos, n, head) \
    __dlist_for_each_rev((pos)->prev, n, head, )


#define __dlist_for_each_rev_safe(pos, n, __prev, head, doit) \
    for (dlist_t *n = pos, *__prev = n->prev; \
         n != (head) && ({ doit; 1; }); \
         n = __prev, __prev = n->prev)

#define dlist_for_each_rev_safe(n, head) \
    __dlist_for_each_rev_safe((head)->prev, n, __prev_##n, head, )
#define dlist_for_each_rev_safe_start(pos, n, head) \
    __dlist_for_each_rev_safe(pos, n, __prev_##n, head, )
#define dlist_for_each_rev_safe_continue(pos, n, head) \
    __dlist_for_each_rev_safe((pos)->prev, n, __prev_##n, head, )


#define __dlist_for_each_entry(pos, n, head, member) \
    __dlist_for_each(pos, __real_##n, head,                        \
                     n = dlist_entry_of(__real_##n, n, member))

#define dlist_for_each_entry(n, head, member) \
    __dlist_for_each_entry((head)->next, n, head, member)
#define dlist_for_each_entry_start(pos, n, head, member) \
    __dlist_for_each_entry(&(pos)->member, n, head, member)
#define dlist_for_each_entry_continue(pos, n, head, member) \
    __dlist_for_each_entry((pos)->member.next, n, head, member)


#define __dlist_for_each_entry_safe(pos, n, head, member) \
    __dlist_for_each_safe(pos, __real_##n, __next_##n, head,  \
                          n = dlist_entry_of(__real_##n, n, member))

#define dlist_for_each_entry_safe(n, head, member) \
    __dlist_for_each_entry_safe((head)->next, n, head, member)
#define dlist_for_each_entry_safe_start(pos, n, head, member) \
    __dlist_for_each_entry_safe(&(pos)->member, n, head, member)
#define dlist_for_each_entry_safe_continue(pos, n, head, member) \
    __dlist_for_each_entry_safe((pos)->member.next, n, head, member)

#endif
