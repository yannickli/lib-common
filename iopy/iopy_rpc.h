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

#ifndef IS_IOPY_CYTHON_RPC_H
#define IS_IOPY_CYTHON_RPC_H

/* XXX: This file should never include <Python.h> */

#include <lib-common/core.h>
#include <lib-common/iop-rpc.h>

/** Result of blocking functions. */
typedef enum iopy_ic_res_t {
    IOPY_IC_OK = 0,
    IOPY_IC_ERR = -1,
    IOPY_IC_SIGINT = -2,
} iopy_ic_res_t;

/* {{{ Server */

/** IC server representation for IOPy. */
typedef struct iopy_ic_server_t iopy_ic_server_t;

/** Create IOPy IC Server.
 *
 * \param[in] ctx The context of the IOPy IC server.
 */
iopy_ic_server_t *iopy_ic_server_create(void *ctx);

/** Destroy IOPy IC server.
 *
 * Stop the IOPy IC server and delete it.
 *
 * \param[in,out] server_ptr The pointer to the server to destroy. Will be set
 *                           to NULL afterwards.
 */
void iopy_ic_server_destroy(iopy_ic_server_t **server_ptr);

/** Start listening IOPy IC server.
 *
 * \param[in]  server The IOPy IC server.
 * \param[in]  uri    The uri the IC server should listen to.
 * \param[out] err    The error description in case of error.
 * \return     -1 in case of error, 0 otherwise.
 */
int iopy_ic_server_listen(iopy_ic_server_t *server, lstr_t uri, sb_t *err);

/** Start listening IOPy IC server and eep listening until time elapsed or
 * server stopped.
 *
 * \param[in]  server  The IOPy IC server.
 * \param[in]  uri     The uri the IC server should listen to.
 * \param[in]  timeout Number of seconds to listen.
 * \param[out] err     The error description in case of error.
 * \return     -1 in case of error, 0 otherwise.
 */
iopy_ic_res_t iopy_ic_server_listen_block(iopy_ic_server_t *server,
                                          lstr_t uri, int timeout, sb_t *err);

/** Stop IOPy IC server.
 *
 * Do nothing if the server is not listening.
 *
 * \param[in] server The IOPy IC server to stop.
 */
void iopy_ic_server_stop(iopy_ic_server_t *server);

/** Register RPC to IOPy IC server.
 *
 * \param[in] server The IOPy IC server.
 * \param[in] rpc    The RPC to register.
 * \param[in] cmd    The command index of the RPC.
 */
void iopy_ic_server_register_rpc(iopy_ic_server_t *server,
                                 const iop_rpc_t *rpc, uint32_t cmd);

/** Unregister RPC from IOPy IC server.
 *
 * \param[in] server The IOPy IC server.
 * \param[in] cmd    The command index of the RPC.
 */
void iopy_ic_server_unregister_rpc(iopy_ic_server_t *server, uint32_t cmd);

/** Called when a peer is connecting to the server.
 *
 * Defined in iopy.pyx.
 *
 * \param[in] ctx         The IOPy IC server context.
 * \param[in] server_uri  The URI the IOPy IC server is listening to.
 * \param[in] remote_addr The address of the peer.
 */
void iopy_ic_server_on_connect(void *ctx, lstr_t server_uri,
                               lstr_t remote_addr);

/** Called when a peer is disconnecting from the server.
 *
 * Defined in iopy.pyx.
 *
 * \param[in] ctx         The IOPy IC server context.
 * \param[in] server_uri  The URI the IOPy IC server is listening to.
 * \param[in] remote_addr The address of the peer.
 */
void iopy_ic_server_on_disconnect(void *ctx, lstr_t server_uri,
                                  lstr_t remote_addr);

/** Called when a request is made to an RPC.
 *
 * Defined in iopy.pyx.
 *
 * \param[in]  ctx    The IOPy IC server context.
 * \param[in]  ic     The ichannel of the request.
 * \param[in]  slot   The slot of the request.
 * \param[in]  arg    The RPC argument of the request.
 * \param[out] res    The RPC result of the reply.
 * \param[out] res_st The IOP type description of the reply.
 * \return  The status of the reply. If the status is not IC_MSG_OK or
 *          IC_MSG_EXN, \p res and \p res_desc are ignored.
 */
ic_status_t t_iopy_ic_server_on_rpc(void *ctx, ichannel_t *ic, uint64_t slot,
                                    void *arg, const ic__hdr__t *hdr,
                                    void **res, const iop_struct_t **res_st);

/* }}} */
/* {{{ Client */

/** IC client representation for IOPy. */
typedef struct iopy_ic_client_t iopy_ic_client_t;

/** Create and connect IOPy IC client.
 *
 * \param[in]  ctx        The IOPy IC client context.
 * \param[in]  uri        The uri the IC client should connect to.
 * \param[in]  timeout    The timeout it should wait for the connection in
 *                        seconds. -1 means forever.
 * \param[out] client_ptr The pointer where to put the new client.
 *                        Will not be set in case of error.
 * \param[out] err        The error description in case of error.
 * \return IOPY_IC_OK if the client has been successfully connected.
 *         IOPY_IC_ERR if an error occured, \p err contains the description of
 *         the error.
 *         IOPY_IC_SIGINT if a sigint occurred during the connection.
 */
iopy_ic_res_t iopy_ic_client_create(void *ctx, lstr_t uri, int timeout,
                                    iopy_ic_client_t **client_ptr, sb_t *err);

/** Destroy IOPy IC client.
 *
 * \param[in,out] client_ptr The pointer to the client to destroy. Will be set
 *                           to NULL afterwards.
 */
void iopy_ic_client_destroy(iopy_ic_client_t **client_ptr);

/** Call RPC with IOPy IC client.
 *
 * \param[in]  client  The IOPy RPC client.
 * \param[in]  rpc     The RPC to call.
 * \param[in]  cmd     The command index of the RPC.
 * \param[in]  hdr     The ic header.
 * \param[in]  timeout The timeout of the request in seconds. -1 means
 *                     forever.
 * \param[in]  arg     The argument value of the RPC.
 * \param[out] status  The query result status.
 * \param[out] res     The query result value. Only set when \p status is set
 *                     to IC_MSG_OK or IC_MSG_EXN. This value is allocated on
 *                     the heap and *MUST* be freed with p_delete().
 * \param[out] err     The error description in case of error.
 * \return IOPY_IC_OK if the query has been run and returned. You must check
 *         if the query has been successful with \p status.
 *         IOPY_IC_ERR if an error occured, \p err contains the description of
 *         the error.
 *         IOPY_IC_SIGINT if a sigint occurred during the query.
 */
iopy_ic_res_t
iopy_ic_client_call(iopy_ic_client_t *client, const iop_rpc_t *rpc,
                    int32_t cmd, const ic__hdr__t *hdr, int timeout,
                    void *arg, ic_status_t *status, void **res, sb_t *err);

/** Returns whether the IOPy IC client is connected or not.
 *
 * \param[in] client The IOPy RPC client.
 * \return true if the associated IC channel is connected, false otherwise
 */
bool iopy_ic_client_is_connected(iopy_ic_client_t *client);

/** Disconnect the IOPy IC client.
 *
 * \param[in] client The IOPy RPC client.
 */
void iopy_ic_client_disconnect(iopy_ic_client_t *client);

/** Called when the client is disconnecting.
 *
 * Defined in iopy.pyx.
 *
 * \param[in] ctx       The IOPy IC client context.
 * \param[in] connected True if the client has been connected before, false
 *                      otherwise.
 */
void iopy_ic_client_on_disconnect(void *ctx, bool connected);

/* }}} */
/* {{{ Module init */

/** Callback called by iopy before fork in the parent process. */
void iopy_rpc_atfork_prepare(void);

/** Callback called by iopy after fork in the parent process. */
void iopy_rpc_atfork_parent(void);

/** Callback called by iopy after fork in the child process. */
void iopy_rpc_atfork_child(void);

/** Init the IOPy RPC C modules. */
void iopy_rpc_module_init(void);

/** Release the IOPy RPC C modules. */
void iopy_rpc_module_shutdown(void);

/* }}} */

#endif /* IS_IOPY_CYTHON_RPC_H */
