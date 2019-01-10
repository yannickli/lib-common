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

#include "hash.h"

/* FIPS 180-2 Validation tests */
static struct {
    const char *name;
    const char *message;
    int len;
    uint32_t crc32;
    const char *sha224_hex;
    const char *sha256_hex;
    const char *sha384_hex;
    const char *sha512_hex;
    const char *md5_hex;
    const char *sha1_hex;
} vectors[] = {
    {
        "m1",
        "abc", 3,
        0x352441c2,
        "23097d223405d8228642a477bda255b32aadbce4bda0b3f7e36c9da7",
        "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad",
        "cb00753f45a35e8bb5a03d699ac65007272c32ab0eded1631a8b605a43ff5bed"
        "8086072ba1e7cc2358baeca134c825a7",
        "ddaf35a193617abacc417349ae20413112e6fa4e89a97ea20a9eeee64b55d39a"
        "2192992a274fc1a836ba3c23a3feebbd454d4423643ce80e2a9ac94fa54ca49f",
        "900150983cd24fb0d6963f7d28e17f72",
        "a9993e364706816aba3e25717850c26c9cd0d89d",
    },
    {
        "m2",
        "abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq", 56,
        0x171a3f5f,
        "75388b16512776cc5dba5da1fd890150b0c6455cb4f58b1952522525",
        "248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1",
        "3391fdddfc8dc7393707a65b1b4709397cf8b1d162af05abfe8f450de5f36bc6"
        "b0455a8520bc4e6f5fe95b1fe3c8452b",
        "204a8fc6dda82f0a0ced7beb8e08a41657c16ef468b228a8279be331a703c335"
        "96fd15c13b1b07f9aa1d3bea57789ca031ad85c7a71dd70354ec631238ca3445",
        "8215ef0796a20bcaaae116d3876c664a",
        "84983e441c3bd26ebaae4aa1f95129e5e54670f1",
    },
    {
        "m3",
        "abcdefghbcdefghicdefghijdefghijkefghij"
        "klfghijklmghijklmnhijklmnoijklmnopjklm"
        "nopqklmnopqrlmnopqrsmnopqrstnopqrstu", 112,
        0x191f3349,
        "c97ca9a559850ce97a04a96def6d99a9e0e0e2ab14e6b8df265fc0b3",
        "cf5b16a778af8380036ce59e7b0492370b249b11e8f07a51afac45037afee9d1",
        "09330c33f71147e83d192fc782cd1b4753111b173b3b05d22fa08086e3b0f712"
        "fcc7c71a557e2db966c3e9fa91746039",
        "8e959b75dae313da8cf4f72814fc143f8f7779c6eb9f7fa17299aeadb6889018"
        "501d289e4900f7e4331b99dec4b5433ac7d329eeb6dd26545e96e55b874be909",
        "03dd8807a93175fb062dfb55dc7d359c",
        "a49b2446a02c645bf419f995b67091253a04a259",
    },
    {
        "m4",
        NULL, 1000000, /* NULL -> this is actually patched later with
                          1.000.000 * 'a' */
        0xdc25bfbc,
        /* SHA-224 */
        "20794655980c91d8bbb4c1ea97618a4bf03f42581948b2ee4ee7ad67",
        /* SHA-256 */
        "cdc76e5c9914fb9281a1c7e284d73e67f1809a48a497200e046d39ccc7112cd0",
        /* SHA-384 */
        "9d0e1809716474cb086e834e310a4a1ced149e9c00f248527972cec5704c2a5b"
        "07b8b3dc38ecc4ebae97ddd87f3d8985",
        /* SHA-512 */
        "e718483d0ce769644e2e42c7bc15b4638e1f98b13b2044285632a803afa973eb"
        "de0ff244877ea60a4cb0432ce577c31beb009c5c2c49aa2e4eadb217ad8cc09b",
        "7707d6ae4e027c70eea2a935c2296f21",
        "34aa973cd4c4daa4f61eeb2bdbad27316534016f",
    },
};

static int testcrc(uint32_t ref, uint32_t result, const char *name)
{
    if (ref == result) {
        printf("0x%08x  %s\n", result, name);
        return 0;
    } else {
        printf("0x%08x  %s  (expected 0x%08x)\n", result, name, ref);
        fprintf(stderr, "Test failed.\n");
        return 1;
    }
}

static int test(const char *vector, const char *digest, const char *name)
{
    if (!strcmp(vector, digest)) {
        printf("%s  %s\n", digest, name);
        return 0;
    } else {
        printf("%s  %s  (expected %s)\n", digest, name, vector);
        fprintf(stderr, "Test failed.\n");
        return 1;
    }
}

int main(void)
{
    char digest[SHA512_DIGEST_SIZE * 2 + 1];
    int i, status = 0;
    void *ptr;

    printf("HASH Validation tests (using SHA-2 FIPS 180-2 Validation vectors)\n");

    ptr = malloc(vectors[3].len);
    if (!ptr) {
        fprintf(stderr, "Cannot allocate %d bytes\n", vectors[3].len);
        return 1;
    }

    memset(ptr, 'a', vectors[3].len);
    vectors[3].message = ptr;

    printf("\nCRC32 Test vectors\n");
    for (i = 0; i < countof(vectors); i++) {
        uint32_t crc = icrc32(0, vectors[i].message, vectors[i].len);
        status |= testcrc(vectors[i].crc32, crc, vectors[i].name);
    }

    printf("\nSHA-224 Test vectors\n");
    for (i = 0; i < countof(vectors); i++) {
        sha224_hex(vectors[i].message, vectors[i].len, digest);
        status |= test(vectors[i].sha224_hex, digest, vectors[i].name);
    }
    printf("\nSHA-256 Test vectors\n");
    for (i = 0; i < countof(vectors); i++) {
        sha256_hex(vectors[i].message, vectors[i].len, digest);
        status |= test(vectors[i].sha256_hex, digest, vectors[i].name);
    }
    printf("\nSHA-384 Test vectors\n");
    for (i = 0; i < countof(vectors); i++) {
        sha384_hex(vectors[i].message, vectors[i].len, digest);
        status |= test(vectors[i].sha384_hex, digest, vectors[i].name);
    }
    printf("\nSHA-512 Test vectors\n");
    for (i = 0; i < countof(vectors); i++) {
        sha512_hex(vectors[i].message, vectors[i].len, digest);
        status |= test(vectors[i].sha512_hex, digest, vectors[i].name);
    }
    printf("\nMD5 Test vectors\n");
    for (i = 0; i < countof(vectors); i++) {
        md5_hex(vectors[i].message, vectors[i].len, digest);
        status |= test(vectors[i].md5_hex, digest, vectors[i].name);
    }
    printf("\nSHA-1 Test vectors\n");
    for (i = 0; i < countof(vectors); i++) {
        sha1_hex(vectors[i].message, vectors[i].len, digest);
        status |= test(vectors[i].sha1_hex, digest, vectors[i].name);
    }

    if (!status) {
        printf("\nAll tests passed.\n");
    }
    free(ptr);
    return status;
}
