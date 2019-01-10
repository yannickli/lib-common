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

#if !defined(IS_LIB_COMMON_CONTAINER_H) || defined(IS_LIB_COMMON_CONTAINER_SLIST_H)
#  error "you must include <lib-common/container.h> instead"
#endif
#define IS_LIB_COMMON_CONTAINER_SLIST_H

struct slist_head {
    struct slist_head *next;
};

static inline void slist_push(struct slist_head *l, struct slist_head *n)
{
    n->next = l->next;
    l->next = n;
}

static inline void slist_pop(struct slist_head *l)
{
    struct slist_head *n = l->next;
    if (n)
        l->next = n->next;
}

static inline struct slist_head slist_cut_after(struct slist_head *l, struct slist_head *n)
{
    struct slist_head res = *l;
    l->next = n->next;
    n->next = NULL;
    return res;
}

static inline struct slist_head **slist_tail(struct slist_head **lp)
{
    while ((*lp)->next)
        lp = &(*lp)->next;
    return lp;
}

static inline void slist_append(struct slist_head **tl, struct slist_head *n)
{
    assert ((*tl)->next == NULL);
    slist_push(*tl, n);
    *tl = n;
}

void __slist_sort(struct slist_head *l,
                  int (*cmp)(const void *, const void *, void *),
                  void *priv, int offset);

#define slist_sort(l, cmp, priv, type_t, member) \
    ({ int (*__cmp)(const type_t *, const type_t *, void *) = (cmp); \
       __slist_sort(l, (void *)__cmp, priv, -offsetof(type_t, member)); })

#define slist_entry(ptr, type, member)  container_of(ptr, type, member)
#define slist_entry_of(ptr, n, member)  slist_entry(ptr, typeof(*n), member)

#define slist_next_entry(e, member) \
    slist_entry(e->member.next, typeof(*e), member)

#define __slist_for_each(n, head, doit) \
     for (struct slist_head *n = (head)->next; \
          n && ({ doit; prefetch(n->next); 1; }); n = n->next)

#define slist_for_each(n, head)   \
    __slist_for_each(n, head, )
#define slist_for_each_entry(n, head, member) \
    __slist_for_each(__real_##n, head, n = slist_entry_of(__real_##n, n, member))


#define __slist_for_each_safe(n, prev, __cur, head, doit) \
    for (struct slist_head *prev = (head), *n = prev->next, *__cur;     \
         (__cur = n = prev->next) && ({ doit; prefetch(n->next); 1; }); \
         prev = __cur == prev->next ? prev->next : prev)

#define slist_for_each_safe(n, prev, head) \
         __slist_for_each_safe(n, prev, __cur_##n, head, )

#define slist_for_each_entry_safe(n, prev, head, member) \
    __slist_for_each_safe(__real_##n, prev, __cur_##n, head, \
                          n = slist_entry_of(__real_##n, n, member))

