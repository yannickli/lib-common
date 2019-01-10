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

#include <sysexits.h>
#include <lib-common/parseopt.h>
#include <lib-common/unix.h>
#include <lib-common/iop-rpc.h>
#include "iop/tstiop.iop.h"

static struct {
    bool  port;
    int   help;
    bool  wsdl;

    el_t  httpd;
    el_t  blocker;

    httpd_trigger__ic_t *itcb;
} httpd_g = {
#define _G  httpd_g
    .port = 1080,
};

static popt_t popts[] = {
    OPT_FLAG('h', "help", &_G.help, "show help"),
    OPT_FLAG('w', "wsdl", &_G.wsdl, "dump wsdl"),
    OPT_INT('p', NULL,   &_G.port, "port to listen to (default: 1080)"),
    OPT_END(),
};

static void on_term(el_t ev, int signo, data_t priv)
{
    el_unregister(&_G.blocker);
}

static void f_cb(IOP_RPC_IMPL_ARGS(tstiop__t, iface, f))
{
    ic_reply(ic, slot, tstiop__t, iface, f, .i = arg->i);
}

#define SCHEMA  "http://example.com/tstiop"

int main(int argc, char **argv)
{
    const char *arg0 = NEXTARG(argc, argv);
    httpd_cfg_t *cfg;
    sockunion_t su = {
        .sin = {
            .sin_family = AF_INET,
            .sin_addr   = { INADDR_ANY },
        }
    };

    e_trace(0, "sizeof(httpd_query_t) = %zd", sizeof(httpd_query_t));

    argc = parseopt(argc, argv, popts, 0);
    if (argc != 0 || _G.help)
        makeusage(EX_USAGE, arg0, "", NULL, popts);

    if (_G.wsdl) {
        SB_8k(sb);

        iop_xwsdl(&sb, &tstiop__t__mod, NULL, SCHEMA, "http://localhost:1080/iop/", false, true);
        return xwrite(STDOUT_FILENO, sb.data, sb.len);
    }

    cfg = httpd_cfg_new();
    httpd_trigger_register(cfg, GET,  "t", httpd_trigger__static_dir_new("/boot"));
    httpd_trigger_register(cfg, HEAD, "t", httpd_trigger__static_dir_new("/boot"));

    _G.itcb = httpd_trigger__ic_new(&tstiop__t__mod, SCHEMA, 2 << 20);
    _G.itcb->query_max_size = 2 << 20;
    httpd_trigger_register(cfg, POST, "iop", &_G.itcb->cb);
    ichttp_register_(_G.itcb, tstiop__t, iface, f, f_cb);

    su.sin.sin_port = htons(_G.port);
    _G.httpd   = httpd_listen(&su, cfg);
    httpd_cfg_delete(&cfg);

    _G.blocker = el_blocker_register();
    el_signal_register(SIGTERM, on_term, NULL);
    el_signal_register(SIGINT,  on_term, NULL);
    el_signal_register(SIGQUIT, on_term, NULL);
    el_loop();
    httpd_unlisten(&_G.httpd);
    return 0;
}
