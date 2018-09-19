#!/bin/sh
##########################################################################
#                                                                        #
#  Copyright (C) 2004-2018 INTERSEC SA                                   #
#                                                                        #
#  Should you receive a copy of this source code, you must check you     #
#  have a proper, written authorization of INTERSEC to hold it. If you   #
#  don't have such an authorization, you must DELETE all source code     #
#  files in your possession, and inform INTERSEC of the fact you obtain  #
#  these files. Should you not comply to these terms, you can be         #
#  prosecuted in the extent permitted by applicable law.                 #
#                                                                        #
##########################################################################

TAGSOPTION="$1"
TAGSOUTPUT="$2"

ctags ${TAGSOPTION} -o ${TAGSOUTPUT} --recurse=yes --totals=yes --links=no \
    --c-kinds=+p --c++-kinds=+p --fields=+liaS --extra=+q \
    \
    --langmap=c:+.blk --langmap=c:+.h --langmap=c++:+.blkk \
    \
    -I 'qv_t qm_t qh_t IOP_RPC_IMPL IOP_RPC_CB qvector_t qhp_min_t qhp_max_t MODULE_BEGIN MODULE_END MODULE_DEPENDS_ON+ MODULE_NEEDED_BY+' \
    --regex-c='/^OBJ_CLASS(_NO_TYPEDEF)?\(+([^,]+),/\2_t/o, cclass/' \
    --regex-c='/^    .*\(\*+([^\ ]+)\)\([a-zA-Z_]+ /\1/x, cmethod/' \
    --regex-c='/^qvector_t\(+([a-zA-Z_]+)\,/\1/t, qvector/' \
    --regex-c='/^q[hm]_k.*_t\(+([a-zA-Z_]+)/\1/t, qhash/' \
    --regex-c='/^MODULE_BEGIN\(+([a-zA-Z_]+)/\1/m, module/' \
    \
    --langdef=iop --langmap=iop:.iop \
    --regex-iop='/^struct +([a-zA-Z]+)/\1/s, struct/' \
    --regex-iop='/^(local +)?(abstract +)?(local +)?class +([a-zA-Z]+)/\4/c, class/' \
    --regex-iop='/^union +([a-zA-Z]+)/\1/u, union/' \
    --regex-iop='/^enum +([a-zA-Z]+)/\1/e, enum/' \
    --regex-iop='/^typedef +[^;]+ +([a-zA-Z]+) *;/\1/t, typedef/' \
    --regex-iop='/^\@ctype\(+([a-zA-Z_]+)\)/\1/t, ctype/' \
    --regex-iop='/^interface +([a-zA-Z]+)/\1/n, interface/' \
    --regex-iop='/^snmpIface +([a-zA-Z]+)/\1/s, snmpiface/' \
    --regex-iop='/^snmpObj +([a-zA-Z]+)/\1/s, snmpobj/' \
    --regex-iop='/^snmpTbl +([a-zA-Z]+)/\1/s, snmptbl/' \
    \
    --exclude=".build*" --exclude=".git" \
    --exclude="*.blk.c" --exclude="*.blkk.cc" \
    --exclude="js/v8" --exclude="ext" --exclude="node_modules/*" \
    \
    --languages='c,c++,iop,python,php,javascript'
