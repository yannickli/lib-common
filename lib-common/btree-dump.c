#include "btree.h"

int main(int argc, char **argv)
{
    btree_t *bt;

    assert(argc == 2);

    bt = btree_open(argv[1], O_RDONLY);
    assert(bt);

    btree_dump(stdout, bt);

    btree_close(&bt);

    return 0;
}
