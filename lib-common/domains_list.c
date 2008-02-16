/**************************************************************************/
/*                                                                        */
/*  Copyright (C) 2004-2008 INTERSEC SAS                                  */
/*                                                                        */
/*  Should you receive a copy of this source code, you must check you     */
/*  have a proper, written authorization of INTERSEC to hold it. If you   */
/*  don't have such an authorization, you must DELETE all source code     */
/*  files in your possession, and inform INTERSEC of the fact you obtain  */
/*  these files. Should you not comply to these terms, you can be         */
/*  prosecuted in the extent permitted by applicable law.                 */
/*                                                                        */
/**************************************************************************/

#include "macros.h"
#include "mem.h"
#include "domains_list.h"

typedef struct node_t {
    /* This is a tree : For each possible down path, we have the index of the
     * next node. Having indexes instead of pointers makes growing easier. */
    int32_t table[40];
    int value;
} node_t;

domains_index_t *domains_index_init(domains_index_t *idx)
{
    idx->nbnodes = 10 * 1024;
    idx->nodes = p_new(node_t, idx->nbnodes);
    if (!idx->nodes) {
        return NULL;
    }
    idx->freenode = 1;
    return idx;
}

void domains_index_wipe(domains_index_t *idx)
{
    p_delete(&idx->nodes);
}

static int node_getkeynb(int c)
{
    static const int keynb[128] = {
     -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1, /*          */
     -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1, /*          */
     -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1, /*          */
     -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1, /*          */
     -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1, /*          */
     -1,  -1,  -1,  -1,  -1,  39,  38,  -1, /*      -.  */
     27,  28,  29,  30,  31,  32,  33,  34, /* 01234567 */
     35,  36,  -1,  -1,  -1,  -1,  -1,  -1, /* 89       */
     -1,   0,   1,   2,   3,   4,   5,   6, /*  ABCDEFG */
      7,   8,   9,  10,  11,  12,  13,  14, /* HIJKLMNO */
     15,  16,  17,  18,  19,  20,  21,  22, /* PQRSTUVW */
     23,  24,  25,  -1,  -1,  -1,  -1,  -1, /* XYZ      */
     -1,   0,   1,   2,   3,   4,   5,   6, /*  abcdefg */
      7,   8,   9,  10,  11,  12,  13,  14, /* hijklmno */
     15,  16,  17,  18,  19,  20,  21,  22, /* pqrstuvw */
     23,  24,  25,  -1,  -1,  -1,  -1,  -1, /* xyz      */
    };
    if (c < 128) {
        return keynb[c & 0x7f];
    } else {
        return -1;
    }
}

int domains_index_add(domains_index_t *idx, const char *key, int value)
{
    int curnode = 0;
    int key_nb;
    int key_pos = strlen(key) - 1;

    /* Index in reverse order because we know the end of the domains are
     * often the same */
    while (key_pos >= 0) {
        key_nb = node_getkeynb(key[key_pos]);
        if (key_nb < 0) {
            return 1;
        }

        if (idx->nodes[curnode].table[key_nb] == 0) {
            idx->nodes[curnode].table[key_nb] = idx->freenode;
            curnode = idx->freenode;
            idx->freenode++;
            if (idx->freenode >= idx->nbnodes) {
                int newnb = idx->nbnodes * 4 / 3;
                p_realloc0(&idx->nodes, idx->nbnodes, newnb);
                idx->nbnodes = newnb;
            }
        } else {
            curnode = idx->nodes[curnode].table[key_nb];
        }
        key_pos--;
    }
    idx->nodes[curnode].value = value;
    return 0;
}

int domains_index_get(const domains_index_t *idx, const char *key, int trace)
{
    int curnode = 0, nextnode;
    int key_nb;

    if (trace) {
        fprintf(stderr, "%30s: ", key);
    }
    while(*key) {
        if (trace) {
            fprintf(stderr, "%d, ", curnode);
        }
        key_nb = node_getkeynb(*key);
        nextnode = idx->nodes[curnode].table[key_nb];
        if (nextnode == 0) {
            return -1;
        } else {
            curnode = nextnode;
        }
        key++;
    }
    if (trace) {
        fprintf(stderr, "\n");
    }
    return idx->nodes[curnode].value;
}

#ifdef STANDALONE

#include <sys/time.h>
#include <time.h>
#include <stdio.h>
#include <string.h>
#include <getopt.h>

#include "timeval.h"

static int is_word(char *buf)
{
    while (node_getkeynb(*buf) >= 0) {
        buf++;
    }
    return (*buf == '\0');
}

static void rtrim(char *buf)
{
    int pos = strlen(buf) - 1;
    while (pos >= 0) {
        if (buf[pos] != '\n' && buf[pos] != '\r' && buf[pos] != ' ') {
            return;
        }
        buf[pos] = '\0';
    }
}

static void stripdown_accents(char *buf)
{
    for (;;) {
        switch (*buf) {
          case '\0': return;
          case 0xe0: *buf = 'a'; break; /* a with grave      */
          case 0xe2: *buf = 'a'; break; /* a with circumflex */
          case 0xe7: *buf = 'c'; break; /* c with cedilla    */
          case 0xe9: *buf = 'e'; break; /* e with acute      */
          case 0xe8: *buf = 'e'; break; /* e with grave      */
          case 0xea: *buf = 'e'; break; /* e with circumflex */
          case 0xee: *buf = 'i'; break; /* i with circumflex */
          case 0xef: *buf = 'i'; break; /* i with diaresis   */
          case 0xf4: *buf = 'o'; break; /* o with circumflex */
          case 0xf9: *buf = 'u'; break; /* u with grave      */
          case 0xfb: *buf = 'u'; break; /* u with circumflex */
        }
        buf++;
    }
}

static int domains_index_populate(domains_index_t *idx, const char *dictname)
{
    FILE *f;
    char buf[256];
    int i;

    f = fopen(dictname, "r");
    if (!f) {
        fprintf(stderr, "ERROR: Unable to open '%s' for reading.\n", dictname);
        return 1;
    }

    i = 1;
    while (fgets(buf, sizeof(buf), f)) {
        rtrim(buf);
        stripdown_accents(buf);
        if (is_word(buf)) {
            domains_index_add(idx, buf, i);
        }
        i++;
    }
    fprintf(stderr, "Successfully read '%s' (%d words)\n", dictname, i);
    return 0;
}

static int domains_index_test_50000(const domains_index_t *idx, const char *word)
{
    struct timeval begin, end, diff;
    int i;
    const char *p, *pend;

    p = (const char *)&idx->nodes[0];
    pend = (const char *)&idx->nodes[idx->freenode - 1];

    while (p < pend) {
        i = *p;
        p++;
    }

    domains_index_get(idx, word, 0);
    gettimeofday(&begin, NULL);
    for (i = 0; i < 50000; i++) {
        domains_index_get(idx, word, 0);
    }
    gettimeofday(&end, NULL);

    diff = timeval_sub(end, begin);

    printf("%30s: %ld,%ld\n", word, diff.tv_sec, diff.tv_usec);
    return 0;
}

static int domains_index_test(const char *dictname, int n)
{
    domains_index_t *idx;

    idx = domains_index_new();
    if (domains_index_populate(idx, dictname)) {
        domains_index_delete(&idx);
        return 1;
    }
    fprintf(stderr, "%d nodes in use\n", idx->freenode);
    fprintf(stderr, "%d nodes allocated\n", idx->nbnodes);
    domains_index_test_50000(idx, "portage");
    domains_index_test_50000(idx, "anticonstitutionnellement");
    domains_index_test_50000(idx, "le");
    domains_index_test_50000(idx, "combustible");
    domains_index_test_50000(idx, "machine");
    domains_index_delete(&idx);
    return 0;
}

static void usage(const char *progname)
{
    fprintf(stderr, "Usage: %s\n", progname);
}

int main(int argc, char **argv)
{
    int c;
    const char *progname = argv[0];
    const char *dictionnary = NULL;
    int n = -1;

    while( (c = getopt(argc, argv, "d:n:h")) != -1) {
        switch(c) {
          case 'd':
            if (dictionnary) {
                fprintf(stderr, "ERROR:Dictionnary already set to '%s'\n",
                        dictionnary);
                return 1;
            }
            dictionnary = optarg;
            break;
          case 'n':
            if (n != -1) {
                fprintf(stderr, "ERROR:nb words already set to %d\n", n);
                return 1;
            }
            n = atoi(optarg);
            break;
          case 'h':
            usage(progname);
            return 0;
        }
    }
    argc -= optind;
    argv += optind;
    if (!dictionnary) {
        dictionnary = "/usr/share/dict/words";
    }
    if (n < 0) {
        n = 100;
    }
    domains_index_test(dictionnary, n);
    return 0;
}
#endif
