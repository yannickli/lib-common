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

#ifndef IS_LIB_COMMON_LIST_H
#define IS_LIB_COMMON_LIST_H

#include <assert.h>
#include <stddef.h>

/*
 * Provides list functions, for objects that have a ->next member.
 *
 * All of this is lexical (meaning that the code is duplicated) but safe as it
 * does not asks (e.g.) that the ->next member has to be the first of the
 * struct.
 *
 * All those code bits are meant to be inlined anyway, so it does not matter
 * right now.
 */

typedef struct generic_list generic_list;
#define LIST_SORT_IS_STABLE  1
void generic_list_sort(generic_list **list,
                       int (*cmp)(generic_list*, generic_list*, void*),
                       void*);

#define SLIST_PROTOS(type, prefix)                                           \
    static inline type **prefix##_list_init(type **list);                    \
    static inline void prefix##_list_wipe(type **list);                      \
    static inline void prefix##_list_push(type **list, type *item);          \
    static inline type *prefix##_list_pop(type **list);                      \
    static inline void prefix##_list_append(type **list, type *item);        \
    static inline void prefix##_list_sort(type **list,                       \
            int (*cmp)(const type *, const type *, void *), void *priv);

#define SLIST_FUNCTIONS(type, prefix)                                        \
    static inline type **prefix##_list_init(type **list) {                   \
        *list = NULL;                                                        \
        return list;                                                         \
    }                                                                        \
    static inline void prefix##_list_push(type **list, type *item) {         \
        item->next = *list;                                                  \
        *list = item;                                                        \
    }                                                                        \
    static inline type *prefix##_list_pop(type **list) {                     \
        STATIC_ASSERT(offsetof(type, next) == 0);                            \
        if (*list) {                                                         \
            type *res = *list;                                               \
            *list = res->next;                                               \
            res->next = NULL;                                                \
            return res;                                                      \
        }                                                                    \
        return NULL;                                                         \
    }                                                                        \
    static inline void prefix##_list_wipe(type **list) {                     \
        while (*list) {                                                      \
            type *item = prefix##_list_pop(list);                            \
            prefix##_delete(&item);                                          \
        }                                                                    \
    }                                                                        \
    static inline void prefix##_list_append(type **list, type *item) {       \
        while (*list) {                                                      \
            list = &(*list)->next;                                           \
        }                                                                    \
        *list = item;                                                        \
    }                                                                        \
    static inline type *prefix##_list_poptail(type *list) {                  \
        if (list) {                                                          \
            type *tmp = list->next;                                          \
            list->next = NULL;                                               \
            return tmp;                                                      \
        }                                                                    \
        return NULL;                                                         \
    }                                                                        \
    static inline void prefix##_list_sort(type **list,                       \
            int (*cmp)(const type *, const type *, void *), void *priv) {    \
        generic_list_sort((generic_list **)list, (void *)cmp, priv);         \
    }

#define DLIST_FUNCTIONS(type, prefix)                                        \
    static inline type *prefix##_list_take(type **list, type *el) {          \
        el->next->prev = el->prev;                                           \
        el->prev->next = el->next;                                           \
                                                                             \
        if (list && *list == el) {                                           \
            *list = (el->next == el) ? NULL : el->next;                      \
        }                                                                    \
                                                                             \
        el->prev = el->next = NULL;                                          \
                                                                             \
        return el;                                                           \
    }                                                                        \
                                                                             \
    static inline type *prefix##_list_prepend(type **list, type *el) {       \
        if (!*list) {                                                        \
            return *list = el->next = el->prev = el;                         \
        }                                                                    \
                                                                             \
        el->next = (*list)->next;                                            \
        el->prev = (*list);                                                  \
                                                                             \
        return (*list = el->prev->next = el->next->prev = el);               \
    }                                                                        \
                                                                             \
    static inline type *prefix##_list_append(type **list, type *el)          \
    {                                                                        \
        if (!*list) {                                                        \
            return *list = el->next = el->prev = el;                         \
        }                                                                    \
                                                                             \
        el->next = (*list);                                                  \
        el->prev = (*list)->prev;                                            \
                                                                             \
        return (el->prev->next = el->next->prev = el);                       \
    }



#endif /* IS_LIB_COMMON_LIST_H */
