/***** THIS FILE IS AUTOGENERATED DO NOT MODIFY DIRECTLY ! *****/
#ifndef IOP_HEADER_GUARD_core_TYPES_H
#define IOP_HEADER_GUARD_core_TYPES_H

#include "core-tdef.iop.h"

extern iop_enum_t const core__log_level__e;
IOP_ENUM(core__log_level);

extern iop_enum_t const core__iop_http_method__e;
IOP_ENUM(core__iop_http_method);

struct core__logger_configuration__t {
    lstr_t   full_name;
    core__log_level__t level;
    bool     force_all;
    bool     is_silent;
};
extern iop_struct_t const core__logger_configuration__s;
IOP_GENERIC(core__logger_configuration);

struct core__log_configuration__t {
    core__log_level__t root_level;
    bool     force_all;
    bool     is_silent;
    core__logger_configuration__array_t specific;
};
extern iop_struct_t const core__log_configuration__s;
IOP_GENERIC(core__log_configuration);

struct core__log_file_configuration__t {
    const iop_struct_t *__vptr;
    int32_t  max_size;
    uint64_t max_time;
    int32_t  max_files;
    int64_t  total_max_size;
    bool     compress;
};
extern iop_struct_t const core__log_file_configuration__s;
IOP_CLASS(core__log_file_configuration);

#define core__log_file_configuration__class_id  0

struct core__licence__t {
    const iop_struct_t *__vptr;
    lstr_t   expires;
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
};
extern iop_struct_t const core__licence__s;
IOP_CLASS(core__licence);

#define core__licence__class_id  0

struct core__signed_licence__t {
    struct core__licence__t *licence;
    lstr_t   signature;
};
extern iop_struct_t const core__signed_licence__s;
IOP_GENERIC(core__signed_licence);

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
extern iop_struct_t const core__httpd_cfg__s;
IOP_GENERIC(core__httpd_cfg);

struct core__httpc_cfg__t {
    uint16_t pipeline_depth;
    uint32_t noact_delay;
    uint32_t max_queries;
    uint32_t on_data_threshold;
    uint32_t header_line_max;
    uint32_t header_size_max;
};
extern iop_struct_t const core__httpc_cfg__s;
IOP_GENERIC(core__httpc_cfg);

#endif
