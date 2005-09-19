#include <stdlib.h>
#include <errno.h>
#include <sys/param.h>
#include <stdarg.h>
#include <stdio.h>

#include "macros.h"
#include "blob.h"
#include "mem.h"
#include "string_is.h"

#define INITIAL_BUFFER_SIZE 32

/*
 * a blob has a vital invariant, making every parse function avoid buffer read
 * overflows.
 *
 * there is *always* a \0 in the data at position len.
 * implyng that size is always >= len+1
 *
 */
typedef struct {
    /* public interface */
    ssize_t len;
    byte * data;

    /* private interface */
    byte * area;   /* originally allocated bloc */
    ssize_t size;  /* allocated size */
} real_blob_t;

static inline real_blob_t *blob_real(blob_t *blob)
{
    return (real_blob_t *)blob;
}

/******************************************************************************/
/* Blob creation / deletion                                                   */
/******************************************************************************/
/*{{{*/

/* create a new, empty buffer */
blob_t * blob_init(blob_t * blob)
{
    real_blob_t * rblob = blob_real(blob);

    rblob->len  = 0;
    rblob->size = INITIAL_BUFFER_SIZE;
    rblob->data = p_new_raw(byte, rblob->size);
    rblob->area = rblob->data;

    rblob->data[rblob->len] = 0;

    return (blob_t*) rblob;
}

/* delete a buffer. the pointer is set to 0 */
void blob_wipe(blob_t * blob)
{
    if (blob) {
        p_delete(&(blob_real(blob)->area));
        blob_real(blob)->data = NULL;
    }
}

/* @see strdup(3) */
blob_t * blob_dup(const blob_t * src)
{
    real_blob_t * dst = p_new_raw(real_blob_t, 1);
    dst->len  = src->len;
    dst->size = MEM_ALIGN(src->len+1);

    dst->data = p_new_raw(byte, dst->size);
    dst->area = dst->data;
    memcpy(dst->data, src->data, src->len+1); /* +1 for the blob_t \0 */

    return (blob_t*)dst;
}

/* XXX unlike strcat(3), blob_cat *creates* a new blob that is the
 * concatenation of two blobs.
 */
blob_t * blob_cat(const blob_t * blob1, const blob_t * blob2)
{
    blob_t * res = blob_dup(blob1);
    blob_append(res, blob2);
    return res;
}

/* resize a blob to the new size.
 *
 * the min(blob->len, newlien) first bytes are preserved
 */
void blob_resize(blob_t * blob, ssize_t newlen)
{
    real_blob_t * rblob = blob_real(blob);
    ssize_t newsize;
    
    if (rblob->size > newlen) {
        rblob->len = newlen;
        rblob->data[rblob->len] = 0;
        return;
    }

    newsize     = MEM_ALIGN(newlen+1);
    if (rblob->data == rblob->area) {
        rblob->data = mem_realloc(rblob->data, newsize);
    } else {
        byte * old_data = rblob->data;
        rblob->data = p_new_raw(byte, newsize);
        memcpy(rblob->data, old_data, blob->len+1); /* +1 for the blob_t \0 */
        p_delete(&rblob->area);
    }
    rblob->area = rblob->data;
    rblob->len  = newlen;
    rblob->size = newsize;

    rblob->data[rblob->len] = 0;
}

/*}}}*/
/******************************************************************************/
/* Blob manipulations                                                         */
/******************************************************************************/
/*{{{*/

/*** private inlines ***/

static inline void
blob_blit_data_real(blob_t * blob, ssize_t pos, const void * data, ssize_t len)
{
    if (len == 0) {
        return;
    }
    if (pos > blob->len) {
        pos = blob->len;
    }
    if (len + pos > blob->len) {
        blob_resize(blob, pos+len);
    }
    memcpy(blob_real(blob)->data + pos, data, len);
}

/* insert `len' data C octets into a blob.
   `pos' gives the posistion in `blob' where `data' should be inserted.

   if the given `pos' is greater than the length of the input octet string,
   the data is appended.
 */
static inline void
blob_insert_data_real(blob_t * blob, ssize_t pos, const void * data, ssize_t len)
{
    ssize_t oldlen = blob->len;

    if (len == 0) {
        return;
    }
    if (pos > blob->len) {
        pos = blob->len;
    }

    blob_resize(blob, blob->len + len);
    if (oldlen > pos) {
        memmove(blob_real(blob)->data + pos + len, blob->data + pos, oldlen - pos);
    }
    memcpy(blob_real(blob)->data + pos, data, len);
}

static inline void
blob_kill_data_real(blob_t * blob, ssize_t pos, ssize_t len)
{
    real_blob_t * rblob = blob_real(blob);
    if (len == 0) {
        return;
    }
    if (pos > rblob->len) {
        return;
    }

    if (pos + len > blob->len) {
        /* in fact, we are truncating the blob */
        rblob->len = pos;
        rblob->data[rblob->len] = '\0';
    } else if (pos == 0) {
        /* in fact, we delete chars at the begining */
        rblob->data += len;
        rblob->size -= len;
        rblob->len  -= len;
    } else {
        /* general case */
        memmove(rblob->data + pos, rblob->data + pos + len,
                rblob->len - pos - len + 1); /* +1 for the blob_t \0 */
        rblob->len  -= len;
    }
}

/*** set functions ***/

void blob_set(blob_t * dest, const blob_t * src)
{
    blob_resize(dest, 0);
    blob_blit_data_real(dest, 0, src->data, src->len);
}

void blob_set_data(blob_t * blob, const void * data, ssize_t len)
{
    blob_resize(blob, 0);
    blob_blit_data_real(blob, 0, data, len);
}

void blob_set_cstr(blob_t * blob, const char * cstr)
{
    blob_resize(blob, 0);
    blob_blit_data_real(blob, 0, cstr, sstrlen(cstr));
}

/*** blit functions ***/

void blob_blit(blob_t * dest, ssize_t pos, const blob_t * src)
{
    blob_blit_data_real(dest, pos, src->data, src->len);
}

void blob_blit_data(blob_t * blob, ssize_t pos, const void * data, ssize_t len)
{
    blob_blit_data_real(blob, pos, data, len);
}

void blob_blit_cstr(blob_t * blob, ssize_t pos, const char * cstr)
{
    blob_blit_data_real(blob, pos, cstr, sstrlen(cstr));
}

/*** insert functions ***/

void blob_insert(blob_t * dest, ssize_t pos, const blob_t * src)
{
    blob_insert_data_real(dest, pos, src->data, src->len);
}

void blob_insert_data(blob_t * blob, ssize_t pos, const void * data, ssize_t len)
{
    blob_insert_data_real(blob, pos, data, len);
}

/* don't insert the NUL ! */
void blob_insert_cstr(blob_t * blob, ssize_t pos, const char * cstr)
{
    blob_insert_data_real(blob, pos, cstr, sstrlen(cstr));
}

/*** append functions ***/

#define BLOB_APPEND_DATA_blob_real(blob, data, data_len)    \
    blob_insert_data_real((blob), (blob)->len, data, data_len)

void blob_append(blob_t * dest, const blob_t * src)
{
    BLOB_APPEND_DATA_blob_real(dest, src->data, src->len);
}

void blob_append_data(blob_t * blob, const void * data, ssize_t len)
{
    BLOB_APPEND_DATA_blob_real(blob, data, len);
}

void blob_append_cstr(blob_t * blob, const char * cstr)
{
    BLOB_APPEND_DATA_blob_real(blob, cstr, sstrlen(cstr));
}

/*** kill functions ***/

void blob_kill_data(blob_t * blob, ssize_t pos, ssize_t len)
{
    blob_kill_data_real(blob, pos, len);
}

void blob_kill_first(blob_t * blob, ssize_t len)
{
    blob_kill_data_real(blob, 0, len);
}

void blob_kill_last(blob_t * blob, ssize_t len)
{
    blob_kill_data_real(blob, blob->len - len, len);
}

/*}}}*/
/******************************************************************************/
/* Blob printf function                                                       */
/******************************************************************************/
/*{{{*/

/* returns the number of bytes written.
   note that blob_printf allways works (or never returns due to memory
   allocation */
ssize_t blob_printf(blob_t *blob, ssize_t pos, const char *fmt, ...)
{
    int size;
    int available;
    real_blob_t * rblob = blob_real(blob);

    if (pos > blob->len) {
        pos = blob->len;
    }
    available = rblob->size - pos;
    
    va_list args;
    va_start(args, fmt);

    size = vsnprintf((char *)rblob->data+pos, available, fmt, args);
    if (size >= available) {
        /* only move the `pos' first bytes in case of realloc */
        rblob->len = pos;
        blob_resize(blob, pos+size);

        va_end(args);
        va_start(args, fmt);
        size = vsnprintf((char*)rblob->data+pos, size+1, fmt, args);
    }
    rblob->len = pos+size;

    va_end(args);

    return size;
}

/* returns the number of bytes written.
   
   negative value means error, without much precision (presumably not enough
   space in the internal buffer, and such an error is permanent.

   though, the buffer is 1Ko long ... and should not be too small */
ssize_t blob_ftime(blob_t *blob, ssize_t pos, const char *fmt, const struct tm *tm)
{
     char buffer[1024];
     size_t res;

     if (pos > blob->len) {
         pos = blob->len;
     }

     /* doc not clear about NUL and I suspect strftime to have very bad
        implementations on some Unices */
     buffer[sizeof(buffer)-1] = '\0';
     if ( (res = strftime(buffer, sizeof(buffer)-1, fmt, tm)) ) {
         blob_resize(blob, pos);
         blob_blit_data_real(blob, pos, buffer, res);
         return res;
     }

     return -1;
}

/*}}}*/
/******************************************************************************/
/* Blob search functions                                                      */
/******************************************************************************/
/*{{{*/

static inline ssize_t
blob_search_data_real(const blob_t *haystack, ssize_t pos, const void *needle, ssize_t len)
{
    byte b;

    if (len == 0) {
        return pos;
    }

    b = *(byte*)needle;

    while (true) {
        while (haystack->data[pos] != b) {
            pos ++;
            if (pos + len > haystack->len) {
                return -1;
            }
        }

        if (memcmp(haystack->data+pos, needle, len)==0) {
            return pos;
        }
        pos ++;
    }
}

/* not very efficent ! */

ssize_t blob_search(const blob_t *haystack, ssize_t pos, const blob_t *needle)
{
    return blob_search_data_real(haystack, pos, needle->data, needle->len);
}

ssize_t blob_search_data(const blob_t *haystack, ssize_t pos, const void *needle, ssize_t len)
{
    return blob_search_data_real(haystack, pos, needle, len);
}

ssize_t blob_search_cstr(const blob_t *haystack, ssize_t pos, const char *needle)
{
    return blob_search_data_real(haystack, pos, needle, sstrlen(needle));
}

/*}}}*/
/******************************************************************************/
/* Blob filtering                                                             */
/******************************************************************************/
/*{{{*/

/* map filter to blob->data[start .. end-1]
   beware that blob->data[end] is not modified !
 */

static inline void
blob_map_range_real(blob_t * blob, ssize_t start, ssize_t end, blob_filter_func_t * filter)
{
    ssize_t i;

    for ( i = start ; i < end ; i++ ) {
        blob_real(blob)->data[i] = filter(blob->data[i]);
    }
}


void blob_map(blob_t * blob, blob_filter_func_t filter)
{
    blob_map_range_real(blob, 0, blob->len, filter);
}

void blob_map_range(blob_t * blob, ssize_t start, ssize_t end, blob_filter_func_t * filter)
{
    blob_map_range_real(blob, start, end, filter);
}


void blob_ltrim(blob_t * blob)
{
    ssize_t i = 0;

    while (isspace(blob->data[i]) && i <= blob->len) i++;
    blob_kill_data_real(blob, 0, i);
}

void blob_rtrim(blob_t * blob)
{
    ssize_t i = blob->len - 1;

    while (isspace(blob->data[i]) && i >= 0) i--;
    blob_kill_data_real(blob, i+1, blob->len);
}

void blob_trim(blob_t * blob)
{
    blob_ltrim(blob);
    blob_rtrim(blob);
}

/*}}}*/
/******************************************************************************/
/* Blob comparisons                                                           */
/******************************************************************************/
/*{{{*/

/* @see memcmp(3) */
int blob_cmp(const blob_t * blob1, const blob_t * blob2)
{
    if (blob1->len == blob2->len) {
        return memcmp(blob1, blob2, blob1->len);
    } else {
        ssize_t len = MIN(blob1->len, blob2->len);
        int     res = memcmp(blob1, blob2, len);
        if (res != 0) {
            return res;
        }
        return (blob1->len == len) ? -1 : 1;
    }
}


int blob_icmp(const blob_t * blob1, const blob_t * blob2)
{
    ssize_t len = MIN(blob1->len, blob2->len);
    ssize_t pos = 0;

    const char * s1 = (const char *)blob1->data;
    const char * s2 = (const char *)blob2->data;

    while (pos < len && tolower(s1[pos]) == tolower(s2[pos])) {
        pos ++;
    }

    /* kludge : remember that blob->data[blob->len] == '\0' */
    return tolower(s1[pos]) - tolower(s2[pos]);
}

int blob_is_equal(const blob_t * blob1, const blob_t * blob2)
{
    if (blob1->len != blob2->len) {
        return false;
    }
    if (blob1->data == blob2->data) {
        /* safe because we know the len are equal */
        return true;
    }
    return (memcmp(blob1->data, blob2->data, blob1->len) == 0);
}

int blob_is_iequal(const blob_t * blob1, const blob_t * blob2)
{
    ssize_t i;

    if (blob1->len != blob2->len) {
        return false;
    }
    if (blob1->data == blob2->data) {
        /* safe because we know the len are equal */
        return true;
    }
    
    /* reverse comp is because strings we compare most often differ at the end
       than at the beginning */
    for (i = blob1->len - 1 ; i >= 0 ; i--) {
        if (tolower(blob1->data[i]) != tolower(blob2->data[i])) {
            return false;
        }
    }
    return true;
}

/*}}}*/
/******************************************************************************/
/* Blob parsing                                                               */
/******************************************************************************/
/*{{{*/

/* try to parse a c-string from the current position in the buffer.

   pos will be incremented to the position right after the NUL
   answer is a pointer *in* the blob, so you have to strdup it if needed.

   @returns :

     positive number or 0
       represent the length of the parsed string (not
       including the leading \0)

     PARSE_EPARSE
       no \0 was found before the end of the blob
 */

ssize_t blob_parse_cstr(const blob_t * blob, ssize_t * pos, const char **answer)
{
    ssize_t walk = *pos;

    while (walk < blob->len) {
        if (blob->data[walk] == '\0') {
            ssize_t len = walk - *pos;
            PARSE_SET_RESULT(answer, (char *)blob->data + *pos);
            *pos    = walk+1;
            return len;
        }
        walk ++;
    }

    return PARSE_EPARSE;
}

/* @returns :

     0         : no error
     PARSE_EPARSE : equivalent to EINVAL for strtol(3)
     PARSE_ERANGE : resulting value out of range.
 */

int blob_parse_long(const blob_t * blob, ssize_t * pos, int base, long *answer)
{
    char *  endptr;
    ssize_t endpos;
    long    number;
    
    number = strtol((char *)blob->data + *pos, &endptr, base);
    endpos = (byte *)endptr - blob->data;

    if (errno == ERANGE) {
        return PARSE_ERANGE;
    }
    if (errno == EINVAL || endpos > blob->len) {
        return PARSE_EPARSE;
    }

    PARSE_SET_RESULT(answer, number);
    *pos    = endpos;
    return PARSE_OK;
    
}

int blob_parse_double(const blob_t * blob, ssize_t * pos, double *answer)
{
    char *  endptr;
    ssize_t endpos;
    double  number;
    
    number = strtod((char *)blob->data + *pos, &endptr);
    endpos = (byte *)endptr - blob->data;

    if (errno == ERANGE) {
        return PARSE_ERANGE;
    }
    if (endpos > blob->len) {
        return PARSE_EPARSE;
    }

    PARSE_SET_RESULT(answer, number);
    *pos    = endpos;
    return PARSE_OK;
}

/*}}}*/
/*[ CHECK ]::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::{{{*/
#ifdef CHECK
/* inlines (check invariants) + setup/teardowns                            {{{*/

static inline void check_blob_invariants(blob_t * blob)
{
    fail_if(blob->len >= blob_real(blob)->size,
            "a blob must have `len < size'. this one has `len = %d' and `size = %d'",
            blob->len, blob_real(blob)->size);
    fail_if(blob->data[blob->len] != '\0', \
            "a blob must have data[len] set to `\\0', `%c' found",
            blob->data[blob->len]);
}

static inline void check_setup(blob_t * blob, const char * data)
{
    blob_init(blob);
    blob_set_cstr(blob, data);
}

static inline void check_teardown(blob_t * blob, blob_t **blob2)
{
    blob_wipe(blob);
    if (blob2 && *blob2) {
        blob_delete(blob2);
    }
}

/*.........................................................................}}}*/
/* tests legacy functions                                                  {{{*/

START_TEST (check_init_wipe)
{
    blob_t blob;
    blob_init(&blob);
    check_blob_invariants(&blob);

    fail_if(blob.len != 0,      "initalized blob MUST have `len' = 0, but has `len = %d'", blob.len);
    fail_if(blob.data == NULL,  "initalized blob MUST have a valid `data'");

    blob_wipe(&blob);
    fail_if(blob.data != NULL,   "wiped blob MUST have `data' set to NULL");
    fail_if(blob.__area != NULL, "wiped blob MUST have `area' set to NULL");
}
END_TEST

START_TEST (check_blob_new)
{
    blob_t * blob = blob_new();

    check_blob_invariants(blob);
    fail_if(blob == NULL,        "no blob was allocated");
    fail_if(blob->len != 0,      "new blob MUST have `len 0', but has `len = %d'", blob->len);
    fail_if(blob->data == NULL,  "new blob MUST have a valid `data'");

    blob_delete(&blob);
    fail_if(blob != NULL, "pointer was not nullified by `blob_delete'");
}
END_TEST

/*.........................................................................}}}*/
/* test set functions                                                      {{{*/

START_TEST (check_set)
{
    blob_t blob, bloub;
    blob_init (&blob);
    blob_init (&bloub);

    /* blob set cstr */
    blob_set_cstr(&blob, "toto");
    check_blob_invariants(&blob);
    fail_if(blob.len != strlen("toto"),
            "blob.len should be %d, but is %d", strlen("toto"), blob.len);
    fail_if(strcmp((const char *)blob.data, "toto") != 0, "blob is not set to `%s'", "toto");

    /* blob set data */
    blob_set_data(&blob, "tutu", strlen("tutu"));
    check_blob_invariants(&blob);
    fail_if(blob.len != strlen("tutu"),
            "blob.len should be %d, but is %d", strlen("tutu"), blob.len);
    fail_if(strcmp((const char *)blob.data, "tutu") != 0, "blob is not set to `%s'", "tutu");

    /* blob set */
    blob_set(&bloub, &blob);
    check_blob_invariants(&bloub);
    fail_if(bloub.len != strlen("tutu"),
            "blob.len should be %d, but is %d", strlen("tutu"), bloub.len);
    fail_if(strcmp((const char *)bloub.data, "tutu") != 0, "blob is not set to `%s'", "tutu");

    blob_wipe(&blob);
    blob_wipe(&bloub);
}
END_TEST

/*.........................................................................}}}*/
/* test blob_dup / blob_cat / blob_resize                                  {{{*/

START_TEST (check_dup)
{
    blob_t blob;
    blob_t * bdup; 

    check_setup(&blob, "toto string");
    bdup = blob_dup(&blob);
    check_blob_invariants(bdup);

    fail_if(bdup->len != blob.len, "duped blob *must* have same len");
    if (memcmp(bdup->data, blob.data, blob.len) != 0) {
        fail("original and dupped blob don't have the same content");
    }

    check_teardown(&blob, &bdup);
}
END_TEST

START_TEST(check_cat)
{
    blob_t b1;
    blob_t * bcat;
    const blob_t * b2 = &b1;

    check_setup(&b1, "toto");
    bcat = blob_cat(&b1, b2);
    check_blob_invariants(bcat);

    fail_if (bcat->len != b1.len + b2->len, 
            "blob_cat-ed blob has not len equal to the sum of the orignal blobs lens");
    if ( memcmp(bcat->data, b1.data, b1.len) !=0 ||
            memcmp (bcat->data + b1.len, b2->data, b2->len) != 0)
    {
        fail("blob_cat-ed blob is not the concatenation of the orginal blobs");
    }

    check_teardown(&b1, &bcat);
}
END_TEST

START_TEST(check_resize)
{
    blob_t b1;
    check_setup(&b1, "tototutu");

    blob_resize(&b1, 4);
    check_blob_invariants(&b1);
    fail_if (b1.len != 4, "blob_resized blob should have len 4, but has %d", b1.len);

    check_teardown(&b1, NULL);
}
END_TEST

/*.........................................................................}}}*/
/* test blit functions                                                     {{{*/

START_TEST (check_blit)
{
    blob_t blob;
    blob_t *b2;

    check_setup(&blob, "toto string");
    b2 = blob_dup(&blob);


    /* blit cstr */
    blob_blit_cstr(&blob, 4, "turlututu");
    check_blob_invariants(&blob);
    fail_if(strcmp((const char *)blob.data, "tototurlututu") != 0, "blit_cstr failed");
    fail_if(blob.len != strlen("tototurlututu"), "blit_cstr failed");

    /* blit data */
    blob_blit_data(&blob, 6, ".:.:.:.", 7);
    check_blob_invariants(&blob);
    fail_if(strcmp((const char *)blob.data, "tototu.:.:.:.") != 0, "blit_data failed");
    fail_if(blob.len != strlen("tototu.:.:.:."), "blit_cstr failed");

    /* blit */
    blob_blit(&blob, blob.len, b2);
    check_blob_invariants(&blob);
    fail_if(strcmp((const char *)blob.data, "tototu.:.:.:.toto string") != 0, "blit_data failed");
    fail_if(blob.len != strlen("tototu.:.:.:.toto string"), "blit_cstr failed");

    check_teardown(&blob, &b2);
}
END_TEST

/*.........................................................................}}}*/
/* test insert functions                                                     {{{*/

START_TEST (check_insert)
{
    blob_t blob;
    blob_t *b2;

    check_setup(&blob, "05");
    b2 = blob_new();
    blob_set_cstr(b2, "67");


    /* insert cstr */
    blob_insert_cstr(&blob, 1, "1234");
    check_blob_invariants(&blob);
    fail_if(strcmp((const char *)blob.data, "012345") != 0, "insert failed");
    fail_if(blob.len != strlen("012345"), "insert failed");

    /* insert data */
    blob_insert_data(&blob, 20, "89", 2);
    check_blob_invariants(&blob);
    fail_if(strcmp((const char *)blob.data, "01234589") != 0, "insert_data failed");
    fail_if(blob.len != strlen("01234589"), "insert_data failed");

    /* insert */
    blob_insert(&blob, 6, b2);
    check_blob_invariants(&blob);
    fail_if(strcmp((const char *)blob.data, "0123456789") != 0, "insert failed");
    fail_if(blob.len != strlen("0123456789"), "insert failed");

    check_teardown(&blob, &b2);
}
END_TEST

/*.........................................................................}}}*/
/* test append functions                                                     {{{*/

START_TEST (check_append)
{
    blob_t blob;
    blob_t *b2;

    check_setup(&blob, "01");
    b2 = blob_new();
    blob_set_cstr(b2, "89");


    /* append cstr */
    blob_append_cstr(&blob, "2345");
    check_blob_invariants(&blob);
    fail_if(strcmp((const char *)blob.data, "012345") != 0, "append failed");
    fail_if(blob.len != strlen("012345"), "append failed");

    /* append data */
    blob_append_data(&blob, "67", 2);
    check_blob_invariants(&blob);
    fail_if(strcmp((const char *)blob.data, "01234567") != 0, "append_data failed");
    fail_if(blob.len != strlen("01234567"), "append_data failed");

    /* append */
    blob_append(&blob, b2);
    check_blob_invariants(&blob);
    fail_if(strcmp((const char *)blob.data, "0123456789") != 0, "append failed");
    fail_if(blob.len != strlen("0123456789"), "append failed");

    check_teardown(&blob, &b2);
}
END_TEST

/*.........................................................................}}}*/
/* test kill functions                                                     {{{*/

START_TEST (check_kill)
{
    blob_t blob;
    check_setup(&blob, "0123456789");

    /* kill first */
    blob_kill_first(&blob, 3);
    check_blob_invariants(&blob);
    fail_if(strcmp((const char *)blob.data, "3456789") != 0, "kill_first failed");
    fail_if(blob.len != strlen("3456789"), "kill_first failed");

    /* kill last */
    blob_kill_last(&blob, 3);
    check_blob_invariants(&blob);
    fail_if(strcmp((const char *)blob.data, "3456") != 0, "kill_last failed");
    fail_if(blob.len != strlen("3456"), "kill_last failed");

    /* kill */
    blob_kill_data(&blob, 1, 2);
    check_blob_invariants(&blob);
    fail_if(strcmp((const char *)blob.data, "36") != 0, "kill failed");
    fail_if(blob.len != strlen("36"), "kill failed");

    check_teardown(&blob, NULL);
}
END_TEST

/*.........................................................................}}}*/
/* test printf functions                                                   {{{*/

START_TEST (check_printf)
{
    char cmp[81];
    blob_t blob;
    check_setup(&blob, "01234");
    snprintf(cmp, 81, "%080i", 0);

    /* printf first */
    blob_printf(&blob, blob.len, "5%s89", "67");
    check_blob_invariants(&blob);
    fail_if(strcmp((const char *)blob.data, "0123456789") != 0, "printf failed");
    fail_if(blob.len != strlen("0123456789"), "printf failed");

    blob_printf(&blob, 0, "%080i", 0);
    check_blob_invariants(&blob);
    fail_if(strcmp((const char *)blob.data, cmp) != 0, "printf failed");
    fail_if(blob.len != sstrlen(cmp), "printf failed");

    check_teardown(&blob, NULL);
}
END_TEST

/*.........................................................................}}}*/
/* test blob_search                                                        {{{*/

START_TEST (check_search)
{
    blob_t blob;
    blob_t *b1 = blob_new();
    check_setup(&blob, "toto string");

    /* search data */
    fail_if(blob_search_data(&blob, 0, (void*)"string", 6) != 5,
            "blob_search fail when needle exists");
    fail_if(blob_search_data(&blob, 0, (void*)"bloube", 6) != -1,
            "blob_search fail when needle doesn't exist");

    /* search cstr */
    fail_if(blob_search_cstr(&blob, 0, "string") != 5,
            "blob_search fail when needle exists");
    fail_if(blob_search_cstr(&blob, 0, "bloube") != -1,
            "blob_search fail when needle doesn't exist");

    /* search */
    blob_set_cstr(b1, "string");
    fail_if(blob_search(&blob, 0, b1) != 5,
            "blob_search fail when needle exists");
    blob_set_cstr(b1, "blouble");
    fail_if(blob_search(&blob, 0, b1) != -1,
            "blob_search fail when needle doesn't exist");

    check_teardown(&blob, &b1);
}
END_TEST

/*.........................................................................}}}*/
/* public testing API                                                      {{{*/

Suite *check_make_blob_suite(void)
{
    Suite *s  = suite_create("Blob");
    TCase *tc = tcase_create("Core");

    suite_add_tcase(s, tc);
    tcase_add_test(tc, check_init_wipe);
    tcase_add_test(tc, check_blob_new);
    tcase_add_test(tc, check_set);
    tcase_add_test(tc, check_dup);
    tcase_add_test(tc, check_cat);
    tcase_add_test(tc, check_blit);
    tcase_add_test(tc, check_insert);
    tcase_add_test(tc, check_append);
    tcase_add_test(tc, check_kill);
    tcase_add_test(tc, check_resize);
    tcase_add_test(tc, check_printf);
    tcase_add_test(tc, check_search);

    return s;
}

/*.........................................................................}}}*/
#endif
/*:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::}}}*/
