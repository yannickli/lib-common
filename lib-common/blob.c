#include <stdlib.h>
#include <errno.h>
#include <sys/param.h>

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

#define REAL(blob) ((real_blob_t*)(blob))

/******************************************************************************/
/* Blob creation / deletion                                                   */
/******************************************************************************/

/* create a new, empty buffer */
blob_t * blob_init(blob_t * blob)
{
    real_blob_t * rblob = REAL(blob);

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
        p_delete(&(REAL(blob)->area));
        REAL(blob)->data = NULL;
    }
}

/* @see strdup(3) */
blob_t * blob_dup(const blob_t * blob)
{
    real_blob_t * src = REAL(blob);
    real_blob_t * dst = p_new_raw(real_blob_t, 1);
    dst->len  = src->len;
    dst->size = MEM_ALIGN(src->size);

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
    real_blob_t * rblob = REAL(blob);
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

/******************************************************************************/
/* Blob manipulations                                                         */
/******************************************************************************/

/*** private inlines ***/

static inline void
blob_blit_data_real(blob_t * blob, ssize_t pos, const void * data, ssize_t len)
{
    if (len + pos > blob->len) {
        blob_resize(blob, pos+len);
    }
    memcpy(REAL(blob)->data + pos, data, len);
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

    blob_resize(blob, blob->len + len);
    if (oldlen > pos) {
        memmove(REAL(blob)->data + pos + len, blob->data + pos, oldlen - pos);
    }
    memcpy(REAL(blob)->data + pos, data, len);
}

static inline void
blob_kill_data_real(blob_t * blob, ssize_t pos, ssize_t len)
{
    real_blob_t * rblob = REAL(blob);
    if (pos > rblob->len) {
        return;
    }

    if (pos + len > blob->len) {
        /* in fact, we are truncating the blob */
        rblob->len = pos;
        rblob->data[rblob->len] = '\0';
    }
    else if (pos == 0) {
        /* in fact, we delete chars at the begining */
        rblob->data += len;
        rblob->size -= len;
    }
    else {
        /* general case */
        memmove(rblob->data + pos, rblob->data + pos + len,
                rblob->len - pos - len + 1); /* +1 for the blob_t \0 */
        rblob->size -= len;
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
    blob_blit_data_real(dest, pos, src, src->len);
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

#define BLOB_APPEND_DATA_REAL(blob, data, data_len)    \
    blob_insert_data_real((blob), (blob)->len, data, data_len)

void blob_append(blob_t * dest, const blob_t * src)
{
    BLOB_APPEND_DATA_REAL(dest, src->data, src->len);
}

void blob_append_data(blob_t * blob, const void * data, ssize_t len)
{
    BLOB_APPEND_DATA_REAL(blob, data, len);
}

void blob_append_cstr(blob_t * blob, const char * cstr)
{
    BLOB_APPEND_DATA_REAL(blob, cstr, sstrlen(cstr));
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

/******************************************************************************/
/* Blob filtering                                                             */
/******************************************************************************/

/* map filter to blob->data[start .. end-1]
   beware that blob->data[end] is not modified !
 */

static inline void
blob_map_range_real(blob_t * blob, ssize_t start, ssize_t end, blob_filter_func_t * filter)
{
    ssize_t i;

    for ( i = start ; i < end ; i++ ) {
        REAL(blob)->data[i] = filter(REAL(blob)->data[i]);
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

    while (isspace(REAL(blob)->data[i]) && i <= blob->len) i++;
    blob_kill_data_real(blob, 0, i);
}

void blob_rtrim(blob_t * blob)
{
    ssize_t i = blob->len - 1;

    while (isspace(REAL(blob)->data[i]) && i >= 0) i--;
    blob_kill_data_real(blob, i+1, blob->len);
}

void blob_trim(blob_t * blob)
{
    blob_ltrim(blob);
    blob_rtrim(blob);
}


/******************************************************************************/
/* Blob comparisons                                                           */
/******************************************************************************/

/* @see memcmp(3) */
int blob_cmp(const blob_t * blob1, const blob_t * blob2)
{
    if (blob1->len == blob2->len) {
        return memcmp(blob1, blob2, blob1->len);
    }
    else {
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

    const char * s1 = (const char *)REAL(blob1)->data;
    const char * s2 = (const char *)REAL(blob2)->data;

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
        if (tolower(REAL(blob1)->data[i]) != tolower(REAL(blob2)->data[i])) {
            return false;
        }
    }
    return true;
}

/******************************************************************************/
/* Blob parsing                                                               */
/******************************************************************************/

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
    real_blob_t * rblob = REAL(blob);
    ssize_t walk = *pos;

    while (walk < blob->len) {
        if (rblob->data[walk] != '\0') {
            ssize_t len = walk - *pos;
            PARSE_SET_RESULT(answer, (char *)rblob->data + *pos);
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

/*******************************************************************************
 * wsp types
 */

int blob_parse_uint8(const blob_t * blob, ssize_t *pos, uint8_t *answer)
{
    PARSE_SET_RESULT(answer, REAL(blob)->data[(*pos)++]);
    return PARSE_OK;
}

int blob_parse_uint16(const blob_t * blob, ssize_t *pos, uint16_t *answer)
{
    if (*pos + 2 > blob->len) {
        return PARSE_EPARSE;
    }
    if (answer != NULL) {
        *answer   = REAL(blob)->data[(*pos)++];
        *answer <<= 8;
        *answer  |= REAL(blob)->data[(*pos)++];
    }
    return PARSE_OK;
}

int blob_parse_uint32(const blob_t * blob, ssize_t *pos, uint32_t *answer)
{
    if (*pos + 4 > blob->len) {
        return PARSE_EPARSE;
    }
    if (answer != NULL) {
        *answer   = REAL(blob)->data[(*pos)++];
        *answer <<= 8;
        *answer  |= REAL(blob)->data[(*pos)++];
        *answer <<= 8;
        *answer  |= REAL(blob)->data[(*pos)++];
        *answer <<= 8;
        *answer  |= REAL(blob)->data[(*pos)++];
    }
    return PARSE_OK;
}

int blob_parse_uintv (const blob_t * blob, ssize_t *pos, uint32_t *answer)
{
    uint32_t value = 0;
    ssize_t  walk  = *pos;
    int      count = 5;

    while (walk < blob->len && count-- > 0) {
        int c = REAL(blob)->data[walk++];
        value  = (value << 7) | (c & 0x7f);
        if ((c & 0x80) == 0) {
            PARSE_SET_RESULT(answer, value);
            *pos    = walk;
            return PARSE_OK;
        }
        if ((value & 0xfe000000) != 0) {
            return PARSE_ERANGE;
        }
    }

    return PARSE_EPARSE;
}

/*[ CHECK ]::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::{{{*/
#ifdef CHECK
/* inlines (check invariants) + setup/teardowns                            {{{*/

static inline void ensure_blob_invariants(blob_t * blob)
{
    fail_if(blob->len >= REAL(blob)->size,
            "a blob must have `len < size'. this one has `len = %d' and `size = %d'",
            blob->len, REAL(blob)->size);
    fail_if(blob->data[blob->len] != '\0', \
            "a blob must have data[len] set to `\\0', `%c' found",
            blob->data[blob->len]);
}

static inline void setup(blob_t * blob, const char * data)
{
    blob_init(blob);
    blob_set_cstr(blob, data);
}

static inline void teardown(blob_t * blob, blob_t **blob2)
{
    blob_wipe(blob);
    if (blob2 && *blob2) {
        blob_delete(blob2);
    }
}

/*.........................................................................}}}*/
/* tests legacy functions                                                  {{{*/

START_TEST (blob_init_wipe)
{
    blob_t blob;
    blob_init(&blob);
    ensure_blob_invariants(&blob);

    fail_if(blob.len != 0,      "initalized blob MUST have `len' = 0, but has `len = %d'", blob.len);
    fail_if(blob.data == NULL,  "initalized blob MUST have a valid `data'");

    blob_wipe(&blob);
    fail_if(blob.data != NULL,   "wiped blob MUST have `data' set to NULL");
    fail_if(blob.__area != NULL, "wiped blob MUST have `area' set to NULL");
}
END_TEST

START_TEST (test_blob_new)
{
    blob_t * blob = blob_new();

    ensure_blob_invariants(blob);
    fail_if(blob == NULL,        "no blob was allocated");
    fail_if(blob->len != 0,      "new blob MUST have `len 0', but has `len = %d'", blob->len);
    fail_if(blob->data == NULL,  "new blob MUST have a valid `data'");

    blob_delete(&blob);
    fail_if(blob != NULL, "pointer was not nullified by `blob_delete'");
}
END_TEST

/*.........................................................................}}}*/
/* test set functions                                                      {{{*/

START_TEST (test_set)
{
    blob_t blob;
    blob_init (&blob);

    blob_set_cstr(&blob, "toto");
    fail_if(blob.len != strlen("toto"),
            "blob.len should be %d, but is %d", strlen("toto"), blob.len);
    fail_if(strcmp((const char *)blob.data, "toto") != 0, "blob is not set to `%s'", "toto");

    blob_wipe(&blob);
}
END_TEST

/*.........................................................................}}}*/
/* test blob_dup / blob_cat / blob_resize                                  {{{*/

START_TEST (test_dup)
{
    blob_t blob;
    blob_t * bdup; 

    setup(&blob, "toto string");
    bdup = blob_dup(&blob);
    ensure_blob_invariants(bdup);

    fail_if(bdup->len != blob.len, "duped blob *must* have same len");
    if (memcmp(bdup->data, blob.data, blob.len) != 0) {
        fail("original and dupped blob don't have the same content");
    }

    teardown(&blob, &bdup);
}
END_TEST

START_TEST(test_cat)
{
    blob_t b1;
    blob_t * bcat;
    const blob_t * b2 = &b1;

    setup(&b1, "toto");
    bcat = blob_cat(&b1, b2);
    ensure_blob_invariants(bcat);

    fail_if (bcat->len != b1.len + b2->len, 
            "blob_cat-ed blob has not len equal to the sum of the orignal blobs lens");
    if ( memcmp(bcat->data, b1.data, b1.len) !=0 ||
            memcmp (bcat->data + b1.len, b2->data, b2->len) != 0)
    {
        fail("blob_cat-ed blob is not the concatenation of the orginal blobs");
    }

    teardown(&b1, &bcat);
}
END_TEST

/*.........................................................................}}}*/
/* public testing API                                                      {{{*/

Suite *make_blob_suite(void)
{
    Suite *s  = suite_create("Blob");
    TCase *tc = tcase_create("Core");

    suite_add_tcase(s, tc);
    tcase_add_test(tc, blob_init_wipe);
    tcase_add_test(tc, test_blob_new);
    tcase_add_test(tc, test_set);
    tcase_add_test(tc, test_dup);
    tcase_add_test(tc, test_cat);

    return s;
}

/*.........................................................................}}}*/
#endif
/*:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::}}}*/
