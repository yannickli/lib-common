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

#if !defined(IS_LIB_COMMON_CONTAINER_H) || defined(IS_LIB_COMMON_CONTAINER_DLIST_H)
#  error "you must include <lib-common/container.h> instead"
#endif
#define IS_LIB_COMMON_CONTAINER_DLIST_H

typedef struct dlist_t {
    struct dlist_t *next, *prev;
} dlist_t;

#define DLIST_INIT(name)  { .next = &(name), .prev = &(name) }
#define DLIST(name)       dlist_t name = DLIST_INIT(name)
static inline void dlist_init(dlist_t *l)
{
    l->next = l->prev = l;
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


static inline void
__dlist_splice(dlist_t *prev, dlist_t *next, dlist_t *src)
{
	dlist_t *first = src->next;
	dlist_t *last = src->prev;

	first->prev = prev;
	prev->next  = first;
	last->next  = next;
	next->prev  = last;
        dlist_init(src);
}

static inline void
dlist_splice(dlist_t *dst, dlist_t *src)
{
	if (!dlist_is_empty(src))
		__dlist_splice(dst, dst->next, src);
}

static inline void
dlist_splice_tail(dlist_t *dst, dlist_t *src)
{
	if (!dlist_is_empty(src))
		__dlist_splice(dst->prev, dst, src);
}


#define dlist_entry(ptr, type, member)  container_of(ptr, type, member)
#define dlist_entry_of(ptr, n, member)  dlist_entry(ptr, typeof(*n), member)

#define dlist_next_entry(e, mber)  dlist_entry(e->mber.next, typeof(*e), mber)
#define dlist_prev_entry(e, mber)  dlist_entry(e->mber.prev, typeof(*e), mber)
#define dlist_first_entry(l, type, mber)  dlist_entry(l->next, type, mber)
#define dlist_last_entry(l, type, mber)   dlist_entry(l->prev, type, mber)

#define __dlist_for_each(pos, n, head, doit) \
     for (dlist_t *n = (pos)->next; \
          n != (head) && ({ doit; prefetch(n->next); }); n = n->next)

#define dlist_for_each(n, head) \
    __dlist_for_each(head, n, head, )
#define dlist_for_each_continue(pos, n, head) \
    __dlist_for_each(pos, n, head, )

#define dlist_for_each_entry(n, head, member) \
    __dlist_for_each(head, __real_##n, head,                         \
                     n = dlist_entry_of(__real_##n, n, member))
#define dlist_for_each_entry_continue(pos, n, head, member) \
    __dlist_for_each(&(pos)->member, __real_##n, head,               \
                     n = dlist_entry_of(__real_##n, n, member))


#define __dlist_for_each_safe(n, __next, head, doit) \
    for (dlist_t *n = (head)->next, *__next = n->next;               \
         n != (head) && ({ doit; prefetch(__next->next); });         \
         n = __next, __next = n->next)

#define dlist_for_each_safe(n, head) \
         __dlist_for_each_safe(n, __next_##n, head, )

#define dlist_for_each_entry_safe(n, head, member) \
    __dlist_for_each_safe(__real_##n, __next_##n, head,              \
                          n = dlist_entry_of(__real_##n, n, member))

