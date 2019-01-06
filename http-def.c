/**************************************************************************/
/*                                                                        */
/*  Copyright (C) 2004-2019 INTERSEC SA                                   */
/*                                                                        */
/*  Should you receive a copy of this source code, you must check you     */
/*  have a proper, written authorization of INTERSEC to hold it. If you   */
/*  don't have such an authorization, you must DELETE all source code     */
/*  files in your possession, and inform INTERSEC of the fact you obtain  */
/*  these files. Should you not comply to these terms, you can be         */
/*  prosecuted in the extent permitted by applicable law.                 */
/*                                                                        */
/**************************************************************************/

#include "http.h"

lstr_t const http_method_str[HTTP_METHOD__MAX] = {
#define V(v) [HTTP_METHOD_##v] = LSTR_IMMED(#v)
    V(OPTIONS),
    V(GET),
    V(HEAD),
    V(POST),
    V(PUT),
    V(DELETE),
    V(TRACE),
    V(CONNECT),
#undef V
};

/* rfc 2616: §6.1.1: Status Code and Reason Phrase */
lstr_t http_code_to_str(http_code_t code)
{
    switch (code) {
#define CASE(c, v)  case HTTP_CODE_##c: return LSTR(v)
        CASE(CONTINUE                , "Continue");
        CASE(SWITCHING_PROTOCOL      , "Switching Protocols");

        CASE(OK                      , "OK");
        CASE(CREATED                 , "Created");
        CASE(ACCEPTED                , "Accepted");
        CASE(NON_AUTHORITATIVE       , "Non-Authoritative Information");
        CASE(NO_CONTENT              , "No Content");
        CASE(RESET_CONTENT           , "Reset Content");
        CASE(PARTIAL_CONTENT         , "Partial Content");

        CASE(MULTIPLE_CHOICES        , "Multiple Choices");
        CASE(MOVED_PERMANENTLY       , "Moved Permanently");
        CASE(FOUND                   , "Found");
        CASE(SEE_OTHER               , "See Other");
        CASE(NOT_MODIFIED            , "Not Modified");
        CASE(USE_PROXY               , "Use Proxy");
        CASE(TEMPORARY_REDIRECT      , "Temporary Redirect");

        CASE(BAD_REQUEST             , "Bad Request");
        CASE(UNAUTHORIZED            , "Unauthorized");
        CASE(PAYMENT_REQUIRED        , "Payment Required");
        CASE(FORBIDDEN               , "Forbidden");
        CASE(NOT_FOUND               , "Not Found");
        CASE(METHOD_NOT_ALLOWED      , "Method Not Allowed");
        CASE(NOT_ACCEPTABLE          , "Not Acceptable");

        CASE(PROXY_AUTH_REQUIRED     , "Proxy Authentication Required");
        CASE(REQUEST_TIMEOUT         , "Request Time-out");
        CASE(CONFLICT                , "Conflict");
        CASE(GONE                    , "Gone");
        CASE(LENGTH_REQUIRED         , "Length Required");
        CASE(PRECONDITION_FAILED     , "Precondition Failed");
        CASE(REQUEST_ENTITY_TOO_LARGE, "Request Entity Too Large");
        CASE(REQUEST_URI_TOO_LARGE   , "Request-URI Too Large");
        CASE(UNSUPPORTED_MEDIA_TYPE  , "Unsupported Media Type");
        CASE(REQUEST_RANGE_UNSAT     , "Requested range not satisfiable");
        CASE(EXPECTATION_FAILED      , "Expectation Failed");
        CASE(TOO_MANY_REQUESTS       , "Too many requests");

        CASE(INTERNAL_SERVER_ERROR   , "Internal Server Error");
        CASE(NOT_IMPLEMENTED         , "Not Implemented");
        CASE(BAD_GATEWAY             , "Bad Gateway");
        CASE(SERVICE_UNAVAILABLE     , "Service Unavailable");
        CASE(GATEWAY_TIMEOUT         , "Gateway Time-out");
        CASE(VERSION_NOT_SUPPORTED   , "HTTP Version not supported");
#undef CASE
    }
    return LSTR("<unknown>");
}
