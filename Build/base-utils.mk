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

#
# mangle path names
#
define fun/path-mangle
$(subst .,_,$(subst /,_,$1))
endef

# gen-and-update-if-changed
#
# $(call fun/gen-and-update-if-changed,<SCRIPT>,<SRCPFX>,<DST>,<ARGS>)
define fun/gen-and-update-if-changed
{ \
TMP=$$(mktemp $2.XXXXXXXXXX) ;\
$1 $4 > $$TMP ;\
if test -f $3 && cmp -s $$TMP $3; then $(RM) $$TMP; else $(MV) $$TMP $3; fi ;\
}
endef

# update-if-changed
#
# Be careful this function is racy, unless <SRC> is guaranteed to be unique
# during parallel executions. You should prefer gen-and-update-if-changed.
#
# $(call fun/update-if-changed,<SRC>,<DST>)
define fun/update-if-changed
if test -f $2 && cmp -s $1 $2; then $(RM) $1; else $(MV) $1 $2; fi
endef

# escape for the shell
#
# $(call fun/sq,<list>)
define fun/sq
$(subst $(var/squote),$(var/squote)\$(var/squote)$(var/squote),$1)
endef

# escape for the shell and make
#
# $(call fun/msq,<list>)
define fun/msq
$(subst $$,$$$$,$(subst $(var/squote),$(var/squote)\$(var/squote)$(var/squote),$1))
endef

# abspath made better (keeps trailing slashes)
#
# $(call fun/abspath,<LIST>)
define fun/abspath
$(addsuffix /,$(abspath $(filter %/,$1))) $(abspath $(filter-out %/,$1))
endef

# rebase a path relative to <PATH> wrt $(var/srcdir)
#
# $(call fun/rebase,<PATH>,<LIST>)
define fun/rebase
$(patsubst $(var/srcdir)/%,%,$(call fun/abspath,$(addprefix $1/,$(filter-out /%,$2)) $(filter /%,$2)))
endef

# build list of exported parameter
#
# Rebase and escape paths
# Preserve variables expansion
#
# $(call fun/exportvars,<PATH>,<LIST>)
define fun/exportvars
$(call fun/msq,$(call fun/rebase,$1,$(filter-out $$%,$2))) $(filter $$%,$2)
endef

# call something once only
#
# $(call fun/do-once,<GUARD>,<CALL>)
define fun/do-once
ifndef once/$1
once/$1 := done
$2
endif
endef

# substitute and only keep matching substs
#
fun/patsubst-filt = $(patsubst $1,$2,$(filter $1,$3))

# Expand to $1 $2 $3 if $2 isn't empty.
#
fun/expand-if2 = $(if $2,$1 $2 $3)

# a space
#
var/space :=
var/space += 

# join with separator
#
# $1 separator
# $2 list
fun/join = $(subst $(var/space),$1,$(strip $2))
