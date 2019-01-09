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

#if !defined(IS_LIB_COMMON_CONTAINER_H) || defined(IS_LIB_COMMON_CONTAINER_LIST_H)
#  error "you must include <lib-common/container.h> instead"
#endif
#define IS_LIB_COMMON_CONTAINER_LIST_H

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
    static inline void prefix##_list_tappend(type_t ***tail, type_t *item) { \
        assert (**tail == NULL);                                             \
        **tail = item;                                                       \
        *tail = &item->next;                                                 \
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
