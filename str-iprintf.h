/**************************************************************************/
/*                                                                        */
/*  Copyright (C) 2004-2017 INTERSEC SA                                   */
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

int iprintf(const char * nonnull format, ...)
        __leaf __attr_printf__(1, 2);
int ifprintf(FILE * nonnull stream, const char * nonnull format, ...)
        __leaf __attr_printf__(2, 3);
int isnprintf(char * nullable str, size_t size,
              const char * nonnull format, ...)
        __leaf __attr_printf__(3, 4);
int ivprintf(const char * nonnull format, va_list arglist)
        __leaf __attr_printf__(1, 0);
int ivfprintf(FILE * nonnull stream, const char * nonnull format,
              va_list arglist)
        __leaf __attr_printf__(2, 0);
int ivsnprintf(char * nullable str, size_t size, const char * nonnull format,
               va_list arglist)
        __leaf __attr_printf__(3, 0);
int ifputs_hex(FILE * nonnull stream, const void * nonnull buf, int len)
        __leaf __attr_nonnull__((2));

int isprintf(char * nullable str, const char * nonnull format, ...)
        __leaf __attr_printf__(2, 3);
int ivsprintf(char * nullable str, const char * nonnull format,
              va_list arglist)
        __leaf __attr_printf__(2, 0);

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

__attr_printf__(1, 0)
static inline char * nonnull ivasprintf(const char * nonnull fmt, va_list ap)

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

__attr_printf__(1, 2)
static inline char * nonnull iasprintf(const char * nonnull fmt, ...)
{
    char *s;
    va_list ap;

    va_start(ap, fmt);
    s = ivasprintf(fmt, ap);
    va_end(ap);

    return s;
}

/* {{{ Formatter registration */

/** Formatter function type.
 *
 * A formatter is called when, for a formatter f, the format %*pf is
 * encountered in the format string. The formatter is then called with the
 * parameters passed as argument of that particular format as well as the
 * modifier.
 *
 * A formatter can either write in a stream or in a buffer, in both cases, it
 * must return the size of the formatted object if it had enough place to
 * write in it. The function must check if the output is supposed to be a
 * stream or a buffer based on the presence of a stream.
 *
 * The buffer may not be large enough to write the received data, and the
 * formatter is not responsible for the appending of a terminating zero.
 *
 * \param[in] modifier  The modifier that triggered the call
 * \param[in] val       The pointer received as parameter.
 * \param[in] val_len   The length received as parameter.
 * \param[out] stream   If the formatter is expected to write on a stream,
 *                      this is the destination stream.
 * \param[out] buf      If the formatter is expected to write in a buffer,
 *                      this is the destination buffer.
 * \param[out] buf_len  The size of the buffer.
 *
 * \return The size required to format the \p val (not including the trailing
 *         zero) or -1 in case of error.
 */
typedef ssize_t
(formatter_f)(int modifier, const void * null_unspecified val, size_t val_len,
              FILE * nullable stream, char * nullable buf, size_t buf_len);

/** Register a formatter for the provided modifier.
 */
__attr_nonnull__((2))
void iprintf_register_formatter(int modifier, formatter_f * nonnull formatter);

/* Formatter helpers. */

/** Write data to file or buffer following a given format.
 *
 * \param[in] fmt  Input format.
 *
 *  \note See \ref formatter_f documentation for other parameters and returned
 *  values.
 */
__attr_printf__(4, 5) __attr_nonnull__((4))
ssize_t formatter_writef(FILE * nullable stream,
                         char * nullable buf, size_t buf_len,
                         const char * nonnull fmt, ...);

/** Write data to file or buffer.
 *
 * \param[in] s    String to write.
 * \param[in] len  String length.
 *
 *  \note See \ref formatter_f documentation for other parameters and returned
 *  values.
 */
ssize_t formatter_write(FILE * nullable stream,
                        char * nullable buf, size_t buf_len,
                        const char * nonnull s, size_t len);


#endif /* IS_LIB_COMMON_STR_IPRINTF_H */
