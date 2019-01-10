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

#include "asn1.h"
#include <lib-inet/sms.h>
#include <lib-inet/ss7-nplan.h>

enum test_tf {
    ASN1_TEST_TRUE = 666,
    ASN1_TEST_FALSE = 667
};

typedef struct {
    int8_t x;
    uint32_t y;
    asn1_opt_int32_t z;
    asn1_int32_vector_t vec;
    asn1_opt_uint32_t t;
    uint64_t u64_1;
    uint64_t u64_2;
    uint64_t u64_3;
    bool     b;
    enum test_tf tf;
} test_0_t;

typedef struct {
    asn1_data_t        opt;
    asn1_string_t      string;
    asn1_data_vector_t vec;
    asn1_bit_string_t  bs;
} test_1_t;

typedef struct {
    test_0_t *opt_t0;
    test_1_t t1;
} test_2_t;

typedef struct {
    asn1_ext_t ph;
    asn1_ext_t ph_opt;
} test_3_t;

typedef struct {
    bool b;
    uint32_t u32;
} test_rdr_rec1_t;

ASN1_DEF_VECTOR(test_rdr_rec1, const test_rdr_rec1_t);
ASN1_DEF_ARRAY(test_rdr_rec1, const test_rdr_rec1_t);

typedef struct {
    asn1_test_rdr_rec1_array_t array;
} simple_array_t;

static test_rdr_rec1_t rec1_vector[] = {
    { .b = true, .u32 = 0x123 },
    { .b = false, .u32 = 0x44444 },
    { .b = false, .u32 = 0x0 },
    { .b = true, .u32 = 0x96 }
};

static test_rdr_rec1_t const *rec1_array[] = {
    &rec1_vector[0],
    &rec1_vector[1],
    &rec1_vector[2],
    &rec1_vector[3],
};

static simple_array_t simple_array = {
    .array = {
        .data = rec1_array,
        .len = 4
    }
};

static uint8_t exp_simple_array[] = {
    0xaa, 0x07, 0xbb, 0x01, 0x01, 0x16, 0x02, 0x01,
    0x23, 0xaa, 0x08, 0xbb, 0x01, 0x00, 0x16, 0x03,
    0x04, 0x44, 0x44, 0xaa, 0x06, 0xbb, 0x01, 0x00,
    0x16, 0x01, 0x00, 0xaa, 0x07, 0xbb, 0x01, 0x01,
    0x16, 0x02, 0x00, 0x96
};

typedef struct {
    asn1_int32_vector_t vec;
} test_rdr_rec2_t;

typedef struct {
    int32_t i1;
    int32_t i2;
    asn1_string_t str;
    asn1_opt_int32_t oi3;
    asn1_bit_string_t bstr;
    test_rdr_rec2_t vec;
    asn1_opt_int32_t oi4;
    test_rdr_rec1_t rec1;
} test_reader_t;

enum choice_type {
    CHOICE_TYPE_1 = 1,
    CHOICE_TYPE_2,
    CHOICE_TYPE_3,
    CHOICE_TYPE_REC1
};

typedef struct {
    enum choice_type type;
    union {
        int32_t choice1;
        int32_t choice2;
        int32_t choice3;
        test_rdr_rec1_t rec1;
    };
} test_choice_t;

ASN1_DEF_VECTOR(test_choice, const test_choice_t);
ASN1_DEF_ARRAY(test_choice, const test_choice_t);

typedef struct {
    int32_t i;
    test_choice_t *choice;
} test_u_choice_t;

typedef struct {
    asn1_test_choice_vector_t choice;
} test_vector_t;

typedef struct {
    asn1_test_choice_array_t choice;
} test_array_t;

typedef struct il_test_t {
    int32_t i1;
    int32_t i2;
} il_test_t;

typedef struct il_test_base_t {
    il_test_t t;
} il_test_base_t;

ASN1_DESC(test_0);
ASN1_DESC(test_1);
ASN1_DESC(test_2);
ASN1_DESC(test_3);

ASN1_DESC_BEGIN(desc, test_0);
    asn1_reg_scalar(desc, test_0, x, 0xab);
    asn1_reg_scalar(desc, test_0, y, 0xcd);
    asn1_reg_scalar(desc, test_0, z, 0xef);
    asn1_reg_scalar(desc, test_0, vec, 0x89);
    asn1_reg_scalar(desc, test_0, t, 0xef);
    asn1_reg_scalar(desc, test_0, u64_1, 0x64);
    asn1_reg_scalar(desc, test_0, u64_2, 0x64);
    asn1_reg_scalar(desc, test_0, u64_3, 0x64);
    asn1_reg_scalar(desc, test_0, b, 0xbb);
    asn1_reg_enum(desc, test_0, test_tf, tf, 0x0f);
ASN1_DESC_END(desc);

ASN1_DESC_BEGIN(desc, test_1);
    asn1_reg_opt_string(desc, test_1, opt, 0x00);
    asn1_reg_string(desc, test_1, string, 0xab);
    asn1_reg_seq_of_string(desc, test_1, vec, 0x75);
    asn1_reg_string(desc, test_1, bs, 0xb5);
ASN1_DESC_END(desc);

ASN1_DESC_BEGIN(desc, test_2);
    asn1_reg_opt_sequence(desc, test_2, test_0, opt_t0, 0x32);
    asn1_reg_sequence(desc, test_2, test_1, t1, 0x34);
ASN1_DESC_END(desc);

ASN1_DESC_BEGIN(desc, test_3);
    asn1_reg_ext(desc, test_3, ph, 0x77);
    asn1_reg_opt_ext(desc, test_3, ph_opt, 0x99);
ASN1_DESC_END(desc);

static ASN1_DESC_BEGIN(desc, test_rdr_rec1);
    asn1_reg_scalar(desc, test_rdr_rec1, b, 0xbb);
    asn1_reg_skip(desc, "test_skip", 0x55);
    asn1_reg_scalar(desc, test_rdr_rec1, u32, 0x16);
ASN1_DESC_END(desc);

static ASN1_SEQUENCE_DESC_BEGIN(desc, simple_array);
    asn1_reg_seq_of_sequence(desc, simple_array, test_rdr_rec1, array, 0xaa);
ASN1_SEQUENCE_DESC_END(desc);

static ASN1_DESC_BEGIN(desc, test_rdr_rec2);
    asn1_reg_scalar(desc, test_rdr_rec2, vec, 0x85);
ASN1_DESC_END(desc);

static ASN1_DESC_BEGIN(desc, test_reader);
    asn1_reg_scalar(desc, test_reader, i1, 0x12);
    asn1_reg_scalar(desc, test_reader, i2, 0x34);
    asn1_reg_string(desc, test_reader, str, 0x82);
    asn1_reg_scalar(desc, test_reader, oi3, 0x56);
    asn1_reg_string(desc, test_reader, bstr, 0x83);
    asn1_reg_sequence(desc, test_reader, test_rdr_rec2, vec, 0xa4);
    asn1_reg_scalar(desc, test_reader, oi4, 0x78);
    asn1_reg_skip(desc, "test_skip", ASN1_TAG_INVALID);
    asn1_reg_sequence(desc, test_reader, test_rdr_rec1, rec1, 0xec);
ASN1_DESC_END(desc);

static ASN1_CHOICE_DESC_BEGIN(desc, test_choice, choice_type, type);
    asn1_reg_scalar(desc, test_choice, choice1, 0x23);
    asn1_reg_scalar(desc, test_choice, choice2, 0x34);
    asn1_reg_scalar(desc, test_choice, choice3, 0x45);
    asn1_reg_sequence(desc, test_choice, test_rdr_rec1, rec1, 0xec);
ASN1_CHOICE_DESC_END(desc);

static ASN1_DESC_BEGIN(desc, test_u_choice);
    asn1_reg_scalar(desc, test_u_choice, i, ASN1_TAG_INTEGER);
    asn1_reg_untagged_choice(desc, test_u_choice, test_choice, choice);
ASN1_DESC_END(desc);

static ASN1_DESC_BEGIN(desc, test_vector);
    asn1_reg_seq_of_untagged_choice(desc, test_vector, test_choice, choice);
ASN1_DESC_END(desc);

static ASN1_DESC_BEGIN(desc, test_array);
    asn1_reg_seq_of_untagged_choice(desc, test_array, test_choice, choice);
ASN1_DESC_END(desc);

static ASN1_DESC_BEGIN(desc, il_test);
    asn1_reg_scalar(desc, il_test, i1, 0x12);
    asn1_reg_scalar(desc, il_test, i2, 0x34);
    asn1_reg_skip  (desc, "skip", 0x55);
ASN1_DESC_END(desc);

static ASN1_DESC_BEGIN(desc, il_test_base);
    asn1_reg_sequence(desc, il_test_base, il_test, t, 0x76);
ASN1_DESC_END(desc);

uint8_t il_test_input[] = {
    0x56, 0x80,
          0x12, 0x02,
                0x10, 0x00,
          0x34, 0x01,
                0x00,
          0x55, 0x80,
                0x04, 0x02,
                      0x12, 0x34,
                0x78, 0x80,
                      0x80, 0x00,
                0x00, 0x00,
          0x00, 0x00,
    0x00, 0x00
};

static int serialize_test_0(uint8_t *dst, const test_0_t *t0)
{
    int32_t length;
    int_vector stack;

    vector_init_pool(&stack, MEM_LIBC);
    length = RETHROW(asn1_pack_size_(t0, asn1_test_0_desc(), &stack));
    asn1_pack_(dst, t0, asn1_test_0_desc(), &stack);
    return length;
}

static int serialize_test_1(uint8_t *dst, const test_1_t *t1)
{
    int32_t length;
    int_vector stack;

    vector_init_pool(&stack, MEM_LIBC);
    length = RETHROW(asn1_pack_size_(t1, asn1_test_1_desc(), &stack));
    asn1_pack_(dst, t1, asn1_test_1_desc(), &stack);
    return length;
}

static int serialize_test_2(uint8_t *dst, const test_2_t *t2)
{
    int32_t length;
    int_vector stack;

    vector_init_pool(&stack, MEM_LIBC);
    length = RETHROW(asn1_pack_size_(t2, asn1_test_2_desc(), &stack));
    asn1_pack_(dst, t2, asn1_test_2_desc(), &stack);
    return length;
}

static int serialize_test_3(uint8_t *dst, const test_3_t *t3)
{
    int32_t length;
    int_vector stack;

    vector_init_pool(&stack, MEM_LIBC);
    length = RETHROW(asn1_pack_size(test_3, t3, &stack));
    asn1_pack(test_3, dst, t3, &stack);
    return length;
}

static const test_choice_t choice_vec[] = {
    ASN1_CHOICE(type, CHOICE_TYPE_2, .choice2 = 0x123),
    ASN1_CHOICE(type, CHOICE_TYPE_1, .choice1 = 0x456),
    ASN1_CHOICE(type, CHOICE_TYPE_3, .choice3 = 0x789),
};

static test_choice_t const * choice_arr[] = {
    &choice_vec[0],
    &choice_vec[1],
    &choice_vec[2],
};

static const uint8_t exp_test_vector[] = {
    0x34, 0x02, 0x01, 0x23, 0x23, 0x02, 0x04, 0x56,
    0x45, 0x02, 0x07, 0x89
};

static const test_vector_t test_vector_in = {
    .choice = {
        .data = choice_vec,
        .len = 3
    }
};

static const test_array_t test_array_in = {
    .choice = {
        .data = choice_arr,
        .len = 3
    }
};

static bool simple_array_equal(const simple_array_t *a1, const simple_array_t *a2)
{
    if (a1->array.len != a2->array.len) {
        return false;
    }

    for (int i = 0; i < a1->array.len; i++) {
        if (a1->array.data[i]->b != a2->array.data[i]->b
        ||  a1->array.data[i]->u32 != a2->array.data[i]->u32)
        {
            return false;
        }
    }
    return true;
}


static bool test_vector_equal(const test_vector_t *t1, const test_vector_t *t2)
{
    if (t1->choice.len != t2->choice.len) {
        return false;
    }

    for (int i = 0; i < t1->choice.len; i++) {
        if (t1->choice.data[i].type != t2->choice.data[i].type) {
            e_trace(0, "FAIL (type %d != %d)", t1->choice.data[i].type,
                    t2->choice.data[i].type);

            return false;
        }
        if (t1->choice.data[i].choice1 != t2->choice.data[i].choice1) /* XXX */
        {
            e_trace(0, "FAIL (choice %d != %d)", t1->choice.data[i].choice1,
                    t2->choice.data[i].choice1);

            return false;
        }
    }

    return true;
}

static bool test_array_equal(const test_array_t *t1, const test_array_t *t2)
{
    if (t1->choice.len != t2->choice.len) {
        return false;
    }

    for (int i = 0; i < t1->choice.len; i++) {
        if (t1->choice.data[i]->type != t2->choice.data[i]->type) {
            e_trace(0, "FAIL (type %d != %d)", t1->choice.data[i]->type,
                    t2->choice.data[i]->type);

            return false;
        }
        if (t1->choice.data[i]->choice1 != t2->choice.data[i]->choice1) /* XXX */
        {
            e_trace(0, "FAIL (choice %d != %d)", t1->choice.data[i]->choice1,
                    t2->choice.data[i]->choice1);

            return false;
        }
    }

    return true;
}

int main(int argc, char **argv)
{
    uint8_t buf[256];
    int len;
    int32_t content[] = { 0x7070, 0x717171 };
    uint8_t bs_content[] = { 0xF };
    bool fail = false;
    pstream_t ps;
    mem_pool_t *mem_pool = mem_stack_pool_new(1024);

    uint8_t expected_0[] = {
        0xab, 0x01, 0xff, 0xcd, 0x05, 0x00, 0x87, 0x65, 0x43, 0x21, 0x89, 0x02,
        0x70, 0x70, 0x89, 0x03, 0x71, 0x71, 0x71, 0xef, 0x01, 0x42, 0x64, 0x05,
        0x00, 0x87, 0x65, 0x43, 0x21, 0x64, 0x09, 0x00, 0x92, 0x34, 0x56, 0x78,
        0x90, 0xab, 0xcd, 0xef, 0x64, 0x08, 0x12, 0x34, 0x56, 0x78, 0x90, 0xab,
        0xcd, 0xef, 0xbb, 0x01, 0x01, 0x0f, 0x02, 0x02, 0x9a
    };

    uint8_t expected_1[] = {
        0xab, 0x06, 0x73, 0x74, 0x72, 0x69, 0x6e, 0x67, 0x55, 0x05, 0x66, 0x69,
        0x72, 0x73, 0x74, 0x55, 0x06, 0x73, 0x65, 0x63, 0x6f, 0x6e, 0x64, 0xb5,
        0x02, 0x04, 0x0f
    };

    uint8_t expected_2[] = {
        0x12, 0x39, 0xab, 0x01, 0xff, 0xcd, 0x05, 0x00, 0x87, 0x65, 0x43, 0x21,
        0x89, 0x02, 0x70, 0x70, 0x89, 0x03, 0x71, 0x71, 0x71, 0xef, 0x01, 0x42,
        0x64, 0x05, 0x00, 0x87, 0x65, 0x43, 0x21, 0x64, 0x09, 0x00, 0x92, 0x34,
        0x56, 0x78, 0x90, 0xab, 0xcd, 0xef, 0x64, 0x08, 0x12, 0x34, 0x56, 0x78,
        0x90, 0xab, 0xcd, 0xef, 0xbb, 0x01, 0x01, 0x0f, 0x02, 0x02, 0x9a, 0x34,
        0x1b, 0xab, 0x06, 0x73, 0x74, 0x72, 0x69, 0x6e, 0x67, 0x55, 0x05, 0x66,
        0x69, 0x72, 0x73, 0x74, 0x55, 0x06, 0x73, 0x65, 0x63, 0x6f, 0x6e, 0x64,
        0xb5, 0x02, 0x04, 0x0f
    };

    uint8_t expected_3[] = {
        0x77, 0x1b, 0xab, 0x06, 0x73, 0x74, 0x72, 0x69, 0x6e, 0x67, 0x55, 0x05,
        0x66, 0x69, 0x72, 0x73, 0x74, 0x55, 0x06, 0x73, 0x65, 0x63, 0x6f, 0x6e,
        0x64, 0xb5, 0x02, 0x04, 0x0f
    };

    test_reader_t exp_rdr_out;
    test_reader_t rdr_out;
    simple_array_t simple_array_out;

    static const uint8_t rdr_bstring[] = { 0x12, 0x58 };

    asn1_data_t vec_content[] = {
        { (uint8_t *)"first", sizeof("first") - 1 },
        { (uint8_t *)"second", sizeof("second") - 1 },
    };


    test_0_t t0 = {
        .x = -1,
        .y = 0x87654321,
        .z = ASN1_OPT_CLEAR(int32),
        .vec = { content, 2 },
        .t = ASN1_OPT_SET(uint32, 0x42),
        .u64_1 = 0x87654321ll,
        .u64_2 = 0x9234567890abcdefll,
        .u64_3 = 0x1234567890abcdefll,
        .b = true,
        .tf = ASN1_TEST_TRUE
    };

    test_1_t t1 = {
        .opt = { NULL, 42 },
        .string = { "string", sizeof("string") - 1 },
        .vec = { vec_content, 2 },
        .bs = { bs_content, 4 },
    };

    test_2_t t2 = { &t0, t1 };

    test_3_t t3 = {
        .ph = { .data = &t1, .desc = ASN1_GET_DESC(test_1) },
        .ph_opt = { .data = NULL }
    };

    test_choice_t exp_choice;
    test_choice_t choice;

    test_choice_t in_u_choice = {
        .type = 2,
        {
            .choice2 = 0x25,
        },
    };

    test_u_choice_t u_choice = {
        .i = 0x34,
        .choice = &in_u_choice
    };

    test_u_choice_t u_choice_out = { .choice = NULL };

    static const uint8_t choice_input[] = {
        0xcc, 0x0d, 0xbb, 0x01, 0x01, 0x55,
        0x04, 0x00, 0x01, 0x02, 0x03, 0x16, 0x2, 0x34, 0x56
    };

    static const uint8_t exp_choice_no_skip[] = {
        0xcc, 0x07, 0xbb, 0x01, 0x01, 0x16, 0x02, 0X34, 0x56
    };

    static const int32_t rdr_vec[] = { 0x1234, 0x8555 };

    static const uint8_t exp_u_choice[] = {
        0x02, 0x01, 0x34, 0x34, 0x01, 0x25
    };

    pstream_t choice_ps = ps_init(choice_input, sizeof(choice_input));
    test_vector_t test_vector;
    test_array_t test_array;
    int_vector stack;
    il_test_base_t il;
    int ret;

    memset(&exp_rdr_out, 0, sizeof(test_reader_t));
    memset(&rdr_out, 0, sizeof(test_reader_t));
    exp_rdr_out = (test_reader_t){
        .i1 = 1234,
        .i2 = 56,
        .str = ASN1_STRSTRING("test"),
        .bstr = ASN1_BIT_STRING(rdr_bstring, 13),
        .oi3 = ASN1_OPT_SET(int32, -0xabcd),
        .oi4 = ASN1_OPT_CLEAR(int32),
        .vec = {
            .vec = {
                .data = rdr_vec,
                .len = 2,
            }
        },
        .rec1 = {
            .b = true,
            .u32 = 0x87785555
        }
    };

    memset(&exp_choice, 0, sizeof(test_choice_t));
    memset(&choice, 0, sizeof(test_choice_t));
    exp_choice.type = CHOICE_TYPE_REC1;
    exp_choice.rec1 = (test_rdr_rec1_t){
        .b = true,
        .u32 = 0x3456
    };

    vector_inita(&stack, 1024);

    len = serialize_test_0(buf, &t0);
    if (len != sizeof(expected_0) || memcmp(expected_0, buf, len)) {
        e_trace(0, "TEST 0 FAILED");
        fail = true;
    }
    e_trace_hex(1, "0: Serialization result", buf, len);

    len = serialize_test_1(buf, &t1);
    if (len != sizeof(expected_1) || memcmp(expected_1, buf, len)) {
        e_trace(0, "TEST 1 FAILED");
        fail = true;
    }
    e_trace_hex(1, "1: Serialization result", buf, len);

    len = serialize_test_2(buf, &t2);
    if (len != sizeof(expected_2) || memcmp(expected_2, buf, len)) {
        e_trace(0, "TEST 2 FAILED");
        fail = true;
    }
    e_trace_hex(1, "2: Serialization result", buf, len);

    len = serialize_test_3(buf, &t3);
    if (len != sizeof(expected_3) || memcmp(expected_3, buf, len)) {
        e_trace(0, "TEST 3 FAILED");
        fail = true;
    }
    e_trace_hex(1, "3: Serialization result", buf, len);

    e_trace(1, "Reader test.");
    len = asn1_pack_size(test_reader, &exp_rdr_out, &stack);
    asn1_pack(test_reader, buf, &exp_rdr_out, &stack);
    ps = ps_init(buf, len);
    if (asn1_unpack(test_reader, &ps, mem_pool, &rdr_out, false) < 0
    || rdr_out.i1 != exp_rdr_out.i1 || rdr_out.i2 != exp_rdr_out.i2
    || rdr_out.str.len != exp_rdr_out.str.len
    || memcmp(rdr_out.str.data, exp_rdr_out.str.data, rdr_out.str.len)
    || rdr_out.bstr.bit_len != exp_rdr_out.bstr.bit_len
    || memcmp(rdr_out.bstr.data, exp_rdr_out.bstr.data,
              asn1_bit_string_size(&rdr_out.bstr) - 1)
    || rdr_out.oi3.has_field != exp_rdr_out.oi3.has_field
    || (rdr_out.oi3.has_field && rdr_out.oi3.v != exp_rdr_out.oi3.v)
    || rdr_out.oi4.has_field != exp_rdr_out.oi4.has_field
    || (rdr_out.oi4.has_field && rdr_out.oi4.v != exp_rdr_out.oi4.v)
    || rdr_out.rec1.b != exp_rdr_out.rec1.b
    || rdr_out.rec1.u32 != exp_rdr_out.rec1.u32
    || rdr_out.vec.vec.len != exp_rdr_out.vec.vec.len
    || memcmp(rdr_out.vec.vec.data, exp_rdr_out.vec.vec.data,
              rdr_out.vec.vec.len))
    {
        e_trace(0, "READER TEST FAILED");
        e_trace_hex(0, "Struct code: ", &rdr_out, (int)sizeof(rdr_out));
        e_trace_hex(0, "Expected struct code: ", &exp_rdr_out,
                    (int)sizeof(exp_rdr_out));
        fail = true;
    }
    e_trace(2, "Reader result: "
            "\ti1 = %d, i2 = %d, rec1 = { b = %d, u32 = %u }"
            "\toi3 = %d",
            rdr_out.i1, rdr_out.i2, rdr_out.rec1.b, rdr_out.rec1.u32,
            rdr_out.oi3.v);

    e_trace(1, "Seq of test (with an array).");
    len = asn1_pack_size(simple_array, &simple_array, &stack);
    asn1_pack(simple_array, buf, &simple_array, &stack);
    if (len != sizeof(exp_simple_array)
    || memcmp(exp_simple_array, buf, len))
    {
        e_trace(0, "SIMPLE ARRAY PACKING FAILED");
        e_trace_hex(0, "got", buf, len);
        e_trace_hex(0, "expected", exp_simple_array,
                    (int)sizeof(exp_simple_array));
        fail = true;
    }
    ps = ps_init(buf, len);
    if (asn1_unpack(simple_array, &ps, mem_pool, &simple_array_out,
                           false) < 0
    || !simple_array_equal(&simple_array_out, &simple_array))
    {
        e_trace(0, "SIMPLE ARRAY UNPACKING FAILED");
        fail = true;
    }

    e_trace(1, "Choice packing test");
    len = asn1_pack_size(test_choice, &exp_choice, &stack);
    asn1_pack(test_choice, buf, &exp_choice, &stack);
    if (len != sizeof(exp_choice_no_skip)
    || memcmp(exp_choice_no_skip, buf, len))
    {
        e_trace(0, "CHOICE PACKING FAILED");
        e_trace_hex(0, "got", buf, len);
        e_trace_hex(0, "expected", exp_choice_no_skip,
                    (int)sizeof(exp_choice_no_skip));
        fail = true;
    }

    e_trace(1, "Choice unpacking test.");
    if (asn1_unpack(test_choice, &choice_ps, NULL, &choice, false) < 0
    || memcmp(&exp_choice, &choice, sizeof(test_choice_t)))
    {
        e_trace(0, "CHOICE UNPACKING FAILED");
        e_trace_hex(0, "Input: ", choice_input, (int) sizeof(choice_input));
        e_trace_hex(0, "Struct code: ", &choice, (int)sizeof(choice));
        e_trace_hex(0, "Expected struct code: ", &exp_choice,
                    (int)sizeof(exp_choice));
        fail = true;
    }

    e_trace(1, "Choice packing test.");
    len = asn1_pack_size(test_u_choice, &u_choice, &stack);
    asn1_pack(test_u_choice, buf, &u_choice, &stack);
    if (len != sizeof(exp_u_choice)
    || memcmp(exp_u_choice, buf, len))
    {
        e_trace(0, "U_CHOICE PACKING FAILED");
        e_trace_hex(0, "got", buf, len);
        e_trace_hex(0, "expected", exp_u_choice, (int)sizeof(exp_u_choice));
        fail = true;
    }

    e_trace(1, "Choice unpacking test.");
    ps = ps_init(buf, len);
    if (asn1_unpack(test_u_choice, &ps, mem_pool, &u_choice_out, false) < 0
    || u_choice.i != u_choice_out.i
    || u_choice.choice->type != u_choice_out.choice->type
    || u_choice.choice->choice2 != u_choice_out.choice->choice2)
    {
        e_trace(0, "U_CHOICE UNPACKING FAILED");
        e_trace(0, "got      { %d, { %d, %d } }", u_choice.i,
                u_choice.choice->type, u_choice.choice->choice2);
        e_trace(0, "expected { %d, { %d, %d } }", u_choice_out.i,
            u_choice_out.choice->type, u_choice_out.choice->choice2);
        fail = true;
    }

    e_trace(1, "Sequence of untagged choice test (with a vector).");
    len = asn1_pack_size(test_vector, &test_vector_in, &stack);
    asn1_pack(test_vector, buf, &test_vector_in, &stack);
    if (len != sizeof(exp_test_vector)
    || memcmp(exp_test_vector, buf, len))
    {
        e_trace(0, "TEST VECTOR FAILED.");
        e_trace_hex(0, "got", buf, len);
        e_trace_hex(0, "expected", exp_test_vector, (int)sizeof(exp_test_vector));
        fail = true;
    }
    ps = ps_init(buf, len);
    if (asn1_unpack(test_vector, &ps, mem_pool, &test_vector, false) < 0
    ||  test_vector.choice.len != 3
    ||  !test_vector_equal(&test_vector, &test_vector_in))
    {
        e_trace(0, "TEST VECTOR UNPACKING FAILED");
        e_trace(0, "got len %d exp %d", test_vector.choice.len, 3);
        fail = true;
    }

    e_trace(1, "Sequence of untagged choice test (with an array).");
    len = asn1_pack_size(test_array, &test_array_in, &stack);
    asn1_pack(test_array, buf, &test_array_in, &stack);
    if (len != sizeof(exp_test_vector)
    || memcmp(exp_test_vector, buf, len))
    {
        e_trace(0, "TEST ARRAY FAILED.");
        e_trace_hex(0, "got", buf, len);
        e_trace_hex(0, "expected", exp_test_vector, (int)sizeof(exp_test_vector));
        fail = true;
    }
    ps = ps_init(buf, len);
    if (asn1_unpack(test_array, &ps, mem_pool, &test_array, false) < 0
    ||  test_array.choice.len != 3
    ||  !test_array_equal(&test_array, &test_array_in))
    {
        e_trace(0, "TEST ARRAY UNPACKING FAILED");
        e_trace(0, "got len %d exp %d", test_array.choice.len, 3);
        fail = true;
    }

    ps = ps_init(il_test_input, sizeof(il_test_input));
    if ((ret = asn1_unpack(il_test_base, &ps, mem_pool, &il, false)) < 0
    ||  il.t.i1 != 0x1000 || il.t.i2 != 0x0)
    {
        e_trace(0, "INDEFINITE LENGTH UNPACKING TEST FAILED");
        if (ret < 0) {
            e_trace(0, "Unpacking error.");
        } else {
            e_trace(0, "Got (%x, %x) expecting (%x, %x)",
                il.t.i1, il.t.i2, 0x1000, 0x0);
        }
        fail = true;
    }

    if (fail) {
        e_trace(0, "\nWARNING: some tests have failed.");
    } else {
        e_trace(0, "\nAll tests successfully passed.");
    }

    return 0;
}

