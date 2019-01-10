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

#if !defined(IS_LIB_COMMON_CORE_H) || defined(IS_LIB_COMMON_STR_IPRINTF_H)
#  error "you must include core.h instead"
#else
#define IS_LIB_COMMON_STR_IPRINTF_H

int iprintf(const char *format, ...)
        __leaf __attr_printf__(1, 2)  __attr_nonnull__((1));
int ifprintf(FILE *stream, const char *format, ...)
        __leaf __attr_printf__(2, 3)  __attr_nonnull__((2));
int isnprintf(char *str, size_t size, const char *format, ...)
        __leaf __attr_printf__(3, 4)  __attr_nonnull__((3));
int ivprintf(const char *format, va_list arglist)
        __leaf __attr_printf__(1, 0)  __attr_nonnull__((1));
int ivfprintf(FILE *stream, const char *format, va_list arglist)
        __leaf __attr_printf__(2, 0)  __attr_nonnull__((2));
int ivsnprintf(char *str, size_t size, const char *format, va_list arglist)
        __leaf __attr_printf__(3, 0)  __attr_nonnull__((3));
int ifputs_hex(FILE *stream, const void *buf, int len)
        __leaf __attr_nonnull__((2));

int isprintf(char *str, const char *format, ...)
        __leaf __attr_printf__(2, 3)  __attr_nonnull__((2));
int ivsprintf(char *str, const char *format, va_list arglist)
        __leaf __attr_printf__(2, 0)  __attr_nonnull__((2));

#if defined(IPRINTF_HIDE_STDIO) && IPRINTF_HIDE_STDIO
#undef sprintf
#define sprintf(...)    isprintf(__VA_ARGS__)
#undef vsprintf
#define vsprintf(...)   ivsprintf(__VA_ARGS__)
#undef printf
#define printf(...)     iprintf(__VA_ARGS__)
#undef fprintf
#define fprintf(...)    ifprintf(__VA_ARGS__)
#undef snprintf
#define snprintf(...)   isnprintf(__VA_ARGS__)
#undef vprintf
#define vprintf(...)    ivprintf(__VA_ARGS__)
#undef vfprintf
#define vfprintf(...)   ivfprintf(__VA_ARGS__)
#undef vsnprintf
#define vsnprintf(...)  ivsnprintf(__VA_ARGS__)
#undef asprintf
#define asprintf(...)   iasprintf(__VA_ARGS__)
#undef vasprintf
#define vasprintf(...)  ivasprintf(__VA_ARGS__)
#endif

__attr_printf__(1, 0)  __attr_nonnull__((1))
static inline char *ivasprintf(const char *fmt, va_list ap)

{
    char buf[BUFSIZ], *s;
    int len;
    va_list ap2;

    va_copy(ap2, ap);
    len = ivsnprintf(buf, ssizeof(buf), fmt, ap2);
    va_end(ap2);

    if (len < ssizeof(buf))
        return (char *)p_dupz(buf, len);

    ivsnprintf(s = p_new(char, len + 1), len + 1, fmt, ap);
    return s;
}

__attr_printf__(1, 2)  __attr_nonnull__((1))
static inline char *iasprintf(const char *fmt, ...)
{
    char *s;
    va_list ap;

    va_start(ap, fmt);
    s = ivasprintf(fmt, ap);
    va_end(ap);

    return s;
}

#endif /* IS_LIB_COMMON_STR_IPRINTF_H */
