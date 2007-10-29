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

static void dump_str_entry(void **a, void *b)
{
    printf("%p %s\n", a, *(const char **)*a);
}

static void hash_filter(void **a, void *b)
{
    const char *s = *a;
    if (s[0] & 1)
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

/* FIXME Those 4 functions are UGLY, use better test code at some point */
static void dump_str_hash(hashtbl_str_t *h)
{
    hashtbl_map((hashtbl_t *)h, &dump_str_entry, NULL);
    printf("-----\n");
}

static void hash_str_filter(void **a, void *b)
{
    const char *s = *(const char **)*a;
    if (s[0] & 1 || b)
        *a = NULL;
}

static void test_hashtbl_str(void)
{
    hashtbl_str_t h = { .name_offs = 0 };
    const char *toto = "toto";
    void **x;

    for (int i = 0; i < 10; i++) {
        WANT(hashtbl_str_insert(&h, i, (void *)&arr[i]) == NULL);
    }
    dump_str_hash(&h);

    WANT((x = hashtbl_str_find(&h, 3, "d")) != NULL);
    hashtbl_remove((hashtbl_t *)(void *)&h, x);
    dump_str_hash(&h);

    hashtbl_str_insert(&h, 3, &toto);
    dump_str_hash(&h);

    hashtbl_map((hashtbl_t *)(void *)&h, &hash_str_filter, NULL);
    dump_str_hash(&h);

    for (int i = 0; i < countof(arr); i++) {
        WANT(hashtbl_str_insert(&h, i + 10, (void *)&arr[i]) == NULL);
    }
    dump_str_hash(&h);

    hashtbl_map((hashtbl_t *)(void *)&h, &hash_str_filter, (void *)1);
    dump_str_hash(&h);

    hashtbl_wipe((hashtbl_t *)(void *)&h);
}

int main(void)
{
    test_hashtbl();
    test_hashtbl_str();
    return 0;
}
