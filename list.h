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

#ifndef IS_LIB_COMMON_LIST_H
#define IS_LIB_COMMON_LIST_H

#include "core.h"

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

#define SLIST_PROTOS(type_t, prefix)                                         \
    static inline type_t **prefix##_list_init(type_t **list);                \
    static inline void prefix##_list_wipe(type_t **list);                    \
    static inline void prefix##_list_push(type_t **list, type_t *item);      \
    static inline type_t *prefix##_list_pop(type_t **list);                  \
    static inline void prefix##_list_append(type_t **list, type_t *item);    \
    static inline void prefix##_list_sort(type_t **list,                     \
            int (*cmp)(const type_t *, const type_t *, void *), void *priv);

#define SLIST_FUNCTIONS(type_t, prefix)                                      \
    static inline type_t **prefix##_list_init(type_t **list) {               \
        *list = NULL;                                                        \
        return list;                                                         \
    }                                                                        \
    static inline void prefix##_list_push(type_t **list, type_t *item) {     \
        item->next = *list;                                                  \
        *list = item;                                                        \
    }                                                                        \
    static inline type_t *prefix##_list_pop(type_t **list) {                 \
        STATIC_ASSERT(offsetof(type_t, next) == 0);                          \
        if (*list) {                                                         \
            type_t *res = *list;                                             \
            *list = res->next;                                               \
            res->next = NULL;                                                \
            return res;                                                      \
        }                                                                    \
        return NULL;                                                         \
    }                                                                        \
    static inline void prefix##_list_wipe(type_t **list) {                   \
        while (*list) {                                                      \
            type_t *item = prefix##_list_pop(list);                          \
            prefix##_delete(&item);                                          \
        }                                                                    \
    }                                                                        \
    static inline void prefix##_list_append(type_t **list, type_t *item) {   \
        while (*list) {                                                      \
            list = &(*list)->next;                                           \
        }                                                                    \
        *list = item;                                                        \
    }                                                                        \
    static inline type_t *prefix##_list_poptail(type_t *list) {              \
        if (list) {                                                          \
            type_t *tmp = list->next;                                        \
            list->next = NULL;                                               \
            return tmp;                                                      \
        }                                                                    \
        return NULL;                                                         \
    }                                                                        \
    static inline void prefix##_list_sort(type_t **list,                     \
            int (*cmp)(const type_t *, const type_t *, void *),              \
            void *priv) {                                                    \
        generic_list_sort((generic_list **)list, (void *)cmp, priv);         \
    }

#define DLIST_FUNCTIONS(type_t, prefix)                                      \
    static inline type_t *prefix##_list_take(type_t **list, type_t *el) {    \
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
    static inline type_t *prefix##_list_append(type_t **list, type_t *el)    \
    {                                                                        \
        if (!*list) {                                                        \
            return *list = el->next = el->prev = el;                         \
        }                                                                    \
                                                                             \
        el->next = (*list);                                                  \
        el->prev = (*list)->prev;                                            \
                                                                             \
        return (el->prev->next = el->next->prev = el);                       \
    }                                                                        \
    static inline type_t *prefix##_list_prepend(type_t **list, type_t *el) { \
        return *list = prefix##_list_append(list, el);                       \
    }                                                                        \
                                                                             \
    static inline type_t *prefix##_list_popfirst(type_t **list) {            \
        return prefix##_list_take(list, *list);                              \
    }                                                                        \
    static inline type_t *prefix##_list_poplast(type_t **list) {             \
        return prefix##_list_take(list, (*list)->prev);                      \
    }



#endif /* IS_LIB_COMMON_LIST_H */
