/**************************************************************************/
/*                                                                        */
/*  Copyright (C) 2004-2014 INTERSEC SA                                   */
/*                                                                        */
/*  Should you receive a copy of this source code, you must check you     */
/*  have a proper, written authorization of INTERSEC to hold it. If you   */
/*  don't have such an authorization, you must DELETE all source code     */
/*  files in your possession, and inform INTERSEC of the fact you obtain  */
/*  these files. Should you not comply to these terms, you can be         */
/*  prosecuted in the extent permitted by applicable law.                 */
/*                                                                        */
/**************************************************************************/

#include "time.h"
#include <dirent.h>
#include "http.h"

static void mime_put_http_ctype(outbuf_t *ob, const char *path)
{
    const char *fpart, *ext;
    bool has_enc = false;
    static const struct {
        int         elen;
        const char *ext;
        const char *ct;
        const char *ce;
    } map[] = {
#define E2(ext, ct, ce)  { sizeof(ext) - 1, ext, ct, ce }
#define E(ext, ct)       E2(ext, ct, NULL)
        E("dbg",   "text/plain"),
        E("cfg",   "text/plain"),
        E("err",   "text/plain"),
        E("log",   "text/plain"),
        E("lst",   "text/plain"),
        E("txt",   "text/plain"),

        E("wsdl",  "text/xml"),
        E("xml",   "text/xml"),
        E("xsd",   "text/xml"),
        E("xsl",   "text/xml"),

        E("htm",   "text/html"),
        E("html",  "text/html"),

        E("pcap",  "application/x-pcap"),

        E("pdf",   "application/pdf"),

        E("tar",   "application/x-tar"),
        E2("tgz",  "application/x-tar", "gzip"),
        E2("tbz2", "application/x-tar", "bzip2"),

        E("rar",   "application/rar"),
        E("zip",   "application/zip"),
#undef E2
#undef E
    };

    t_push();
    fpart = path_filepart(path);
    fpart = t_dupz(fpart, strlen(fpart));
    ext   = path_extnul(fpart);
    if (strequal(ext, ".gz")) {
        ob_adds(ob, "Content-Encoding: gzip\r\n");
        has_enc = true;
        *(char *)ext = '\0';
        ext = path_extnul(fpart);
    } else
    if (strequal(ext, ".Z")) {
        ob_adds(ob, "Content-Encoding: compress\r\n");
        has_enc = true;
        *(char *)ext = '\0';
        ext = path_extnul(fpart);
    } else
    if (strequal(ext, ".bz2")) {
        ob_adds(ob, "Content-Encoding: bzip2\r\n");
        has_enc = true;
        *(char *)ext = '\0';
        ext = path_extnul(fpart);
    }

    if (*ext++ == '.') {
        int extlen = strlen(ext);

        for (int i = 0; i < countof(map); i++) {
            if (map[i].elen == extlen && !strcasecmp(map[i].ext, ext)) {
                ob_addf(ob, "Content-Type: %s\r\n", map[i].ct);
                if (!has_enc && map[i].ce)
                    ob_addf(ob, "Content-Encoding: %s\r\n", map[i].ce);
                goto done;
            }
        }
    }
    ob_adds(ob, "Content-Type: application/octet-stream\r\n");
  done:
    t_pop();
}
static void httpd_query_reply_make_index_(httpd_query_t *q, int dfd,
                                         const struct stat *st, bool head)
{
    DIR *dir = fdopendir(dfd);
    struct dirent *de;
    outbuf_t *ob;

    if (!dir) {
        httpd_reject(q, NOT_FOUND, "");
        return;
    }

    ob = httpd_reply_hdrs_start(q, HTTP_CODE_OK, false);
    httpd_put_date_hdr(ob, "Last-Modified", st->st_mtime);
    ob_adds(ob, "Content-Type: text/html\r\n");
    httpd_reply_hdrs_done(q, -1, true);
    if (!head) {
        httpd_reply_chunk_start(q, ob);

        ob_adds(ob, "<html><body><h1>Index</h1>");

        rewinddir(dir);
        t_push();
        de = t_new_extra(struct dirent, fpathconf(dfd, _PC_NAME_MAX) + 1);
        while (readdir_r(dir, de, &de) == 0 && de) {
            struct stat tmp;

            if (de->d_name[0] == '.')
                continue;
            if (fstatat(dfd, de->d_name, &tmp, AT_SYMLINK_NOFOLLOW))
                continue;
            if (S_ISDIR(tmp.st_mode)) {
                ob_addf(ob, "<a href=\"%s/\">%s/</a><br>", de->d_name, de->d_name);
            } else
            if (S_ISREG(tmp.st_mode)) {
                ob_addf(ob, "<a href=\"%s\">%s</a><br>", de->d_name, de->d_name);
            }
        }
        t_pop();
        closedir(dir);

        ob_adds(ob, "</body></html>");
        httpd_reply_chunk_done(q, ob);
    }
    httpd_reply_done(q);
}

void httpd_reply_make_index(httpd_query_t *q, int dfd, bool head)
{
    struct stat st;

    if (fstat(dfd, &st)) {
        httpd_reject(q, NOT_FOUND, "");
    } else {
        httpd_query_reply_make_index_(q, dup(dfd), &st, head);
    }
}

void httpd_reply_file(httpd_query_t *q, int dfd, const char *file, bool head)
{
    int fd = openat(dfd, file, O_RDONLY);
    struct stat st;
    outbuf_t *ob;
    void *map = NULL;

    if (fd < 0)
        goto ret404;

    if (fstat(fd, &st))
        goto ret404;
    if (S_ISDIR(st.st_mode)) {
        if (file[strlen(file) - 1] != '/')
            goto ret404;
        httpd_reply_make_index(q, fd, head);
        return;
    }
    if (!S_ISREG(st.st_mode))
        goto ret404;
    if (!head && st.st_size > (16 << 10)) {
        map = mmap(NULL, st.st_size, PROT_READ, MAP_SHARED, fd, 0);
        if (map == MAP_FAILED) {
            httpd_reject(q, INTERNAL_SERVER_ERROR, "mmap failed: %m");
            close(fd);
            return;
        }
        madvise(map, st.st_size, MADV_SEQUENTIAL);
    }

    ob = httpd_reply_hdrs_start(q, HTTP_CODE_OK, false);
    httpd_put_date_hdr(ob, "Last-Modified", st.st_mtime);
    if (st.st_mtime >= lp_getsec() - 10) {
        ob_addf(ob, "ETag: W/\"%"PRIx64"-%"PRIx64"x-%lx\"\r\n",
                st.st_ino, st.st_size, st.st_mtime);
    } else {
        ob_addf(ob, "ETag: \"%"PRIx64"-%"PRIx64"x-%lx\"\r\n",
                st.st_ino, st.st_size, st.st_mtime);
    }
    mime_put_http_ctype(ob, file);
    httpd_reply_hdrs_done(q, st.st_size, false);
    if (!head) {
        if (map) {
            ob_add_memmap(ob, map, st.st_size);
        } else {
            ob_xread(ob, fd, st.st_size);
        }
    }
    httpd_reply_done(q);
    close(fd);
    return;

  ret404:
    if (fd >= 0)
        close(fd);
    httpd_reject(q, NOT_FOUND, "");
}


typedef struct trigger_cb__dir_t {
    httpd_trigger_cb_t cb;
    int  dirlen;
    char dirpath[];
} trigger_cb__dir_t;

static void trigger_cb__dir_cb(httpd_trigger_cb_t *cb, httpd_query_t *q,
                               const httpd_qinfo_t *req)
{
    trigger_cb__dir_t *cb2 = container_of(cb, trigger_cb__dir_t, cb);
    char *buf;

    t_push();
    buf = t_new_raw(char, cb2->dirlen + ps_len(&req->query) + 1);
    memcpyz(mempcpy(buf, cb2->dirpath, cb2->dirlen),
            req->query.s, ps_len(&req->query));
    httpd_reply_file(q, AT_FDCWD, buf, req->method == HTTP_METHOD_HEAD);
    t_pop();
}

httpd_trigger_cb_t *httpd_trigger__static_dir(const char *path)
{
    int len   = strlen(path);
    trigger_cb__dir_t *cbdir = p_new_extra(trigger_cb__dir_t, len + 1);

    while (len > 0 && path[len - 1] == '/')
        len--;
    cbdir->cb.cb  = &trigger_cb__dir_cb;
    cbdir->dirlen = len;
    memcpy(cbdir->dirpath, path, len + 1);
    return &cbdir->cb;
}
