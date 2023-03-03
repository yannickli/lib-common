#!/bin/bash
##########################################################################
#                                                                        #
#  Copyright (C) INTERSEC SA                                             #
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

ctags_has_lang() {
    ctags --list-languages | grep -iqF "$1"
}

TAGS_TYPESCRIPT_DEF=(
    --langdef=typescript
    --regex-typescript='/^[ \t]*(export[ \t]+([a-z]+[ \t]+)?)?class[ \t]+([a-zA-Z0-9_$]+)/\3/c,classes/'
    --regex-typescript='/^[ \t]*(declare[ \t]+)?namespace[ \t]+([a-zA-Z0-9_$]+)/\2/c,modules/'
    --regex-typescript='/^[ \t]*(export[ \t]+)?module[ \t]+([a-zA-Z0-9_$]+)/\2/n,modules/'
    --regex-typescript='/^[ \t]*(export[ \t]+)?(async[ \t]+)?function[ \t]+([a-zA-Z0-9_$]+)/\3/f,functions/'
    --regex-typescript='/^[ \t]*export[ \t]+(var|let|const)[ \t]+([a-zA-Z0-9_$]+)/\2/v,variables/'
    --regex-typescript='/^[ \t]*(var|let|const)[ \t]+([a-zA-Z0-9_$]+)[ \t]*=[ \t]*function[ \t]*[*]?[ \t]*\(\)/\2/v,varlambdas/'
    --regex-typescript='/^[ \t]*(export[ \t]+)?(public|protected|private)[ \t]+(static[ \t]+)?(abstract[ \t]+)?(((get|set)[ \t]+)|(async[ \t]+[*]*[ \t]*))?([a-zA-Z1-9_$]+)/\9/m,members/'
    --regex-typescript='/^[ \t]*(export[ \t]+)?interface[ \t]+([a-zA-Z0-9_$]+)/\2/i,interfaces/'
    --regex-typescript='/^[ \t]*(export[ \t]+)?type[ \t]+([a-zA-Z0-9_$]+)/\2/t,types/'
    --regex-typescript='/^[ \t]*(export[ \t]+)?enum[ \t]+([a-zA-Z0-9_$]+)/\2/e,enums/'
    --regex-typescript='/^[ \t]*import[ \t]+([a-zA-Z0-9_$]+)/\1/I,imports/'
)

if ctags_has_lang "typescript"; then
    # Typescript is already builtin in ctags no need to redefine it (which is
    # actually forbidden with Universal Ctags)
    TAGS_TYPESCRIPT_DEF=()
fi

ctags ${TAGSOPTION} -o ${TAGSOUTPUT} --recurse=yes --totals=yes --links=no \
    --c-kinds=+p --c++-kinds=+p --fields=+liaS --extra=+q \
    \
    --langmap=c:+.blk --langmap=c:+.h --langmap=c++:+.blkk \
    \
    -I 'qv_t qm_t qh_t IOP_RPC_IMPL IOP_RPC_CB qvector_t qhp_min_t qhp_max_t MODULE_BEGIN MODULE_END MODULE_DEPENDS_ON+ MODULE_NEEDED_BY+' \
    --regex-c='/^OBJ_CLASS(_NO_TYPEDEF)?\(+([^,]+),/\2_t/o,cclass/' \
    --regex-c='/^    .*\(\*+([^\ ]+)\)\([a-zA-Z_]+ /\1/X,cmethod/' \
    --regex-c='/^qvector_t\(+([a-zA-Z_]+)\,/\1/t,typedef/' \
    --regex-c='/^q[hm]_k.*_t\(+([a-zA-Z_]+)/\1/t,typedef/' \
    --regex-c='/^MODULE_BEGIN\(+([a-zA-Z_]+)/\1/M,module/' \
    \
    --langdef=iop --langmap=iop:.iop \
    --regex-iop='/^struct +([a-zA-Z]+)/\1/s,struct/' \
    --regex-iop='/^(local +)?(abstract +)?(local +)?class +([a-zA-Z]+)/\4/c,class/' \
    --regex-iop='/^union +([a-zA-Z]+)/\1/u,union/' \
    --regex-iop='/^enum +([a-zA-Z]+)/\1/e,enum/' \
    --regex-iop='/^typedef +[^;]+ +([a-zA-Z]+) *;/\1/t,typedef/' \
    --regex-iop='/^\@ctype\(+([a-zA-Z_]+)\)/\1/C,ctype/' \
    --regex-iop='/^interface +([a-zA-Z]+)/\1/n,interface/' \
    --regex-iop='/^snmpIface +([a-zA-Z]+)/\1/S,snmpobj/' \
    --regex-iop='/^snmpObj +([a-zA-Z]+)/\1/S,snmpobj/' \
    --regex-iop='/^snmpTbl +([a-zA-Z]+)/\1/S,snmpobj/' \
    \
    --langmap=javascript:+.jsx \
    "${TAGS_TYPESCRIPT_DEF[@]}" \
    --langmap=typescript:+.ts \
    --langmap=typescript:+.tsx \
    \
    --exclude=".build*" --exclude=".git" \
    --exclude="*.blk.c" --exclude="*.blkk.cc" \
    --exclude="js/v8" --exclude="ext" --exclude="node_modules/*" \
    \
    --languages='c,c++,iop,python,php,javascript,typescript'
