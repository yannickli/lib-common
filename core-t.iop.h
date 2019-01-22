/***** THIS FILE IS AUTOGENERATED DO NOT MODIFY DIRECTLY ! *****/
#ifndef IOP_HEADER_GUARD_core_TYPES_H
#define IOP_HEADER_GUARD_core_TYPES_H

#include "core-tdef.iop.h"

#if __has_feature(nullability)
#pragma GCC diagnostic push
#pragma GCC diagnostic error "-Wnullability-completeness"
#endif

EXPORT iop_enum_t const core__log_level__e;
EXPORT iop_enum_t const * const nonnull core__log_level__ep;

EXPORT iop_enum_t const core__iop_http_method__e;
EXPORT iop_enum_t const * const nonnull core__iop_http_method__ep;

struct core__logger_configuration__t {
    lstr_t   full_name;
    core__log_level__t level;
    bool     force_all;
    bool     is_silent;
};
EXPORT iop_struct_t const core__logger_configuration__s;
EXPORT iop_struct_t const * const nonnull core__logger_configuration__sp;
struct core__log_configuration__t {
    core__log_level__t root_level;
    bool     force_all;
    bool     is_silent;
    core__logger_configuration__array_t specific;
};
EXPORT iop_struct_t const core__log_configuration__s;
EXPORT iop_struct_t const * const nonnull core__log_configuration__sp;
struct core__log_file_configuration__t {
    const iop_struct_t *nonnull __vptr;
    int32_t  max_size;
    uint64_t max_time;
    int32_t  max_files;
    int64_t  total_max_size;
    bool     compress;
};
EXPORT iop_struct_t const core__log_file_configuration__s;
EXPORT iop_struct_t const * const nonnull core__log_file_configuration__sp;
#define core__log_file_configuration__class_id  0

struct core__licence_module__t {
    const iop_struct_t *nonnull __vptr;
    lstr_t           expiration_date;
    uint32_t expiration_warning_delay;
};
EXPORT iop_struct_t const core__licence_module__s;
EXPORT iop_struct_t const * const nonnull core__licence_module__sp;
#define core__licence_module__class_id  0

struct core__licence__t {
    const iop_struct_t *nonnull __vptr;
    lstr_t   expiration_date;
    lstr_t           expiration_hard_date;
    uint32_t expiration_warning_delay;
    lstr_t   registered_to;
    lstr_t   version;
    bool     production_use;
#ifndef IOP_ARRAY_T
    IOP_ARRAY_OF(int64_t)  cpu_signatures;
#else
    iop_array_i64_t    cpu_signatures;
#endif
#ifndef IOP_ARRAY_T
    IOP_ARRAY_OF(lstr_t)   mac_addresses;
#else
    iop_array_lstr_t   mac_addresses;
#endif
    core__licence_module__array_t modules;
    opt_i64_t        signature_ts;
};
EXPORT iop_struct_t const core__licence__s;
EXPORT iop_struct_t const * const nonnull core__licence__sp;
#define core__licence__class_id  0

struct core__activation_token__t {
    lstr_t           expiration_date;
    lstr_t   token;
};
EXPORT iop_struct_t const core__activation_token__s;
EXPORT iop_struct_t const * const nonnull core__activation_token__sp;
struct core__signed_licence__t {
    struct core__licence__t *nonnull licence;
    lstr_t   signature;
    struct core__activation_token__t *nullable activation_token;
    core__iop_json_subfile__array_t included_subfiles;
};
EXPORT iop_struct_t const core__signed_licence__s;
EXPORT iop_struct_t const * const nonnull core__signed_licence__sp;
struct core__httpd_cfg__t {
    lstr_t   bind_addr;
    uint32_t outbuf_max_size;
    uint16_t pipeline_depth;
    uint32_t noact_delay;
    uint32_t max_queries;
    uint32_t max_conns_in;
    uint32_t on_data_threshold;
    uint32_t header_line_max;
    uint32_t header_size_max;
};
EXPORT iop_struct_t const core__httpd_cfg__s;
EXPORT iop_struct_t const * const nonnull core__httpd_cfg__sp;
struct core__httpc_cfg__t {
    uint16_t pipeline_depth;
    uint32_t noact_delay;
    uint32_t max_queries;
    uint32_t on_data_threshold;
    uint32_t header_line_max;
    uint32_t header_size_max;
};
EXPORT iop_struct_t const core__httpc_cfg__s;
EXPORT iop_struct_t const * const nonnull core__httpc_cfg__sp;
struct core__iop_json_subfile__t {
    lstr_t   file_path;
    lstr_t   iop_path;
};
EXPORT iop_struct_t const core__iop_json_subfile__s;
EXPORT iop_struct_t const * const nonnull core__iop_json_subfile__sp;
typedef core__iop_json_subfile__t iop_json_subfile__t;
typedef core__iop_json_subfile__array_t iop_json_subfile__array_t;
#define iop_json_subfile__s  core__iop_json_subfile__s

#if __has_feature(nullability)
#pragma GCC diagnostic pop
#endif

#endif
