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

struct dlist_head {
    struct dlist_head *next, *prev;
};

#define DLIST_HEAD_INIT(name)  { .next = &(name), .prev = &(name) }
static inline void dlist_head_init(struct dlist_head *l)
{
    l->next = l->prev = l;
}

static inline void __dlist_add(struct dlist_head *e, struct dlist_head *prev,
                               struct dlist_head *next)
{
    next->prev = e;
    e->next = next;
    e->prev = prev;
    prev->next = e;
}
static inline void
__dlist_remove(struct dlist_head *prev, struct dlist_head *next)
{
    next->prev = prev;
    prev->next = next;
}

static inline void dlist_add(struct dlist_head *l, struct dlist_head *n) {
    __dlist_add(n, l, l->next);
}
static inline void dlist_add_tail(struct dlist_head *l, struct dlist_head *n) {
    __dlist_add(n, l->prev, l);
}

static inline void dlist_remove(struct dlist_head *l) {
    __dlist_remove(l->prev, l->next);
    dlist_head_init(l);
}
static inline void dlist_pop(struct dlist_head *l) {
    dlist_remove(l->next);
}

static inline void dlist_move(struct dlist_head *l, struct dlist_head *n) {
    __dlist_remove(n->prev, n->next);
    dlist_add(l, n);
}
static inline void dlist_move_tail(struct dlist_head *l, struct dlist_head *n) {
    __dlist_remove(n->prev, n->next);
    dlist_add_tail(l, n);
}

static inline bool
dlist_is_first(const struct dlist_head *l, const struct dlist_head *n) {
    return n->prev == l;
}
static inline bool
dlist_is_last(const struct dlist_head *l, const struct dlist_head *n) {
    return n->next == l;
}
static inline bool dlist_is_empty(const struct dlist_head *l) {
    return l->next == l;
}


static inline void
__dlist_splice(struct dlist_head *prev, struct dlist_head *next, struct dlist_head *src)
{
	struct dlist_head *first = src->next;
	struct dlist_head *last = src->prev;

	first->prev = prev;
	prev->next  = first;
	last->next  = next;
	next->prev  = last;
        dlist_head_init(src);
}

static inline void
dlist_splice(struct dlist_head *dst, struct dlist_head *src)
{
	if (!dlist_is_empty(src))
		__dlist_splice(dst, dst->next, src);
}

static inline void
dlist_splice_tail(struct dlist_head *dst, struct dlist_head *src)
{
	if (!dlist_is_empty(src))
		__dlist_splice(dst->prev, dst, src);
}


#define dlist_entry(ptr, type, member)  container_of(ptr, type, member)
#define dlist_entry_of(ptr, n, member)  dlist_entry(ptr, typeof(*n), member)

#define dlist_next_entry(e, member) \
    dlist_entry(e->member.next, typeof(*e), member)

#define __dlist_for_each(n, head, doit) \
     for (struct dlist_head *n = (head)->next; \
          n != (head) && ({ doit; prefetch(n->next); }); n = n->next)

#define dlist_for_each(n, head)   \
    __dlist_for_each(n, head, )
#define dlist_for_each_entry(n, head, member) \
    __dlist_for_each(__real_##n, head, n = dlist_entry_of(__real_##n, n, member))


#define __dlist_for_each_safe(n, __next, head, doit) \
    for (struct dlist_head *n = (head)->next, *__next = n->next;      \
         n != (head) && ({ doit; prefetch(__next->next); });          \
         n = __next, __next = n->next)

#define dlist_for_each_safe(n, head) \
         __dlist_for_each_safe(n, __next_##n, head, )

#define dlist_for_each_entry_safe(n, head, member) \
    __dlist_for_each_safe(__real_##n, __next_##n, head,         \
                          n = dlist_entry_of(__real_##n, n, member))

