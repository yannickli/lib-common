##########################################################################
#                                                                        #
#  Copyright (C) 2004-2008 INTERSEC SAS                                  #
#                                                                        #
#  Should you receive a copy of this source code, you must check you     #
#  have a proper, written authorization of INTERSEC to hold it. If you   #
#  don't have such an authorization, you must DELETE all source code     #
#  files in your possession, and inform INTERSEC of the fact you obtain  #
#  these files. Should you not comply to these terms, you can be         #
#  prosecuted in the extent permitted by applicable law.                 #
#                                                                        #
##########################################################################

# update-if-changed
#
# $(call fun/update-if-changed,<SRC>,<DST>)
define fun/update-if-changed
if test -f $2 && cmp --quiet $1 $2; then $(RM) $1; else mv $1 $2; fi
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

# rebase a path relative to <PATH> wrt $(var/srcdir)
#
# $(call fun/rebase,<PATH>,<LIST>)
define fun/rebase
$(patsubst $(var/srcdir)/%,%,$(abspath $(addprefix $1/,$(filter-out /%,$2)) $(filter /%,$2)))
endef

# call something once only
#
# $(call fun/do-once,<GUARD>,<CALL>)
define fun/do-once
$(if $(once/$1),,
once/$1 := done
$2
)
endef
