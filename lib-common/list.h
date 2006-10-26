/**************************************************************************/
/*                                                                        */
/*  Copyright (C) 2004-2006 INTERSEC                                      */
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

#define SLIST_FUNCTIONS(type, prefix)                                        \
    static inline type *prefix##_list_pop(type **list) {                     \
        if (*list) {                                                         \
            type *res = *list;                                               \
            *list = res->next;                                               \
            res->next = NULL;                                                \
            return res;                                                      \
        }                                                                    \
        return NULL;                                                         \
    }                                                                        \
    static inline void prefix##_list_push(type **list, type *item) {         \
        item->next = *list;                                                  \
        *list = item;                                                        \
    }                                                                        \
                                                                             \
    static inline type **prefix##_list_init(type **list) {                   \
        *list = NULL;                                                        \
        return list;                                                         \
    }                                                                        \
    static inline void prefix##_list_wipe(type **list, bool del) {           \
        if (del) {                                                           \
            while (*list) {                                                  \
                type *item = prefix##_list_pop(list);                        \
                prefix##_delete(&item);                                      \
            }                                                                \
        } else {                                                             \
            *list = NULL;                                                    \
        }                                                                    \
    }                                                                        \



#endif /* IS_LIB_COMMON_LIST_H */
