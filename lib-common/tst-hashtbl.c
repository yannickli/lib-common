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
    if (s[0] & 1)
        *a = NULL;
}

int main(void)
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

    hashtbl_wipe(&h);
    return 0;
}
