#include "hashtbl.c"

static const char * const arr[] = {
    "a", "b", "c", "d", "e", "f", "g", "h",
    "i", "j", "k", "l", "m", "n", "o", "p",
    "q", "r", "s", "t", "u", "v", "w", "x",
};

#define WANT(expr)                          \
    do if (!(expr)) {                       \
        fprintf(stderr, "%s\n", #expr);     \
        abort();                            \
    } while (0);

static void dump_entry(void **a, void *b)
{
    printf("%p %s\n", a, (const char *)*a);
}

static void dump_hash(hashtbl_t *h)
{
    hashtbl_map(h, &dump_entry, NULL);
    printf("-----\n");
}

static void hash_filter(void **a, void *b)
{
    const char *s = *a;
    if (s[0] & 1 || b)
        *a = NULL;
}

static void test_hashtbl(void)
{
    hashtbl_t h;
    void **x;

    hashtbl_init(&h);
    for (int i = 0; i < 10; i++) {
        WANT(hashtbl_insert(&h, i, (void *)arr[i]) == NULL);
    }
    dump_hash(&h);

    WANT((x = hashtbl_find(&h, 3)) != NULL);
    hashtbl_remove(&h, x);
    dump_hash(&h);

    hashtbl_insert(&h, 3, (void *)"toto");
    dump_hash(&h);

    hashtbl_map(&h, &hash_filter, NULL);
    dump_hash(&h);

    for (int i = 0; i < countof(arr); i++) {
        WANT(hashtbl_insert(&h, i + 10, (void *)arr[i]) == NULL);
    }
    dump_hash(&h);

    hashtbl_map(&h, &hash_filter, NULL);
    dump_hash(&h);

    hashtbl_wipe(&h);
}

/* needed for newer gcc's that doesn't like the char * unconst thing for some
   reason */
static inline char *unconst(const char *s) {
    intptr_t i = (intptr_t)s;
    return (char *)i;
}

static void test_hashtbl_str(void)
{
    string_hash h;
    char **x;

    string_hash_init(&h);

    for (int i = 0; i < 10; i++) {
        uint64_t k = string_hash_hkey(arr[i], -1);
        WANT(string_hash_insert(&h, k, unconst(arr[i])) == NULL);
    }
    dump_hash((hashtbl_t *)(void *)&h);

    WANT((x = string_hash_find(&h, string_hash_hkey("d", 1), "d")) != NULL);
    string_hash_remove(&h, x);
    dump_hash((hashtbl_t *)(void *)&h);

    string_hash_map(&h, (void *)&hash_filter, NULL);
    dump_hash((hashtbl_t *)(void *)&h);

    for (int i = 0; i < countof(arr); i++) {
        WANT(string_hash_insert(&h, i + 10, unconst(arr[i])) == NULL);
    }
    dump_hash((hashtbl_t *)(void *)&h);

    string_hash_map(&h, (void *)&hash_filter, (void *)1);
    dump_hash((hashtbl_t *)(void *)&h);

    string_hash_wipe(&h);
}

int main(void)
{
    test_hashtbl();
    test_hashtbl_str();
    return 0;
}
