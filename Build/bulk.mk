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
# extension driven rules
# ~~~~~~~~~~~~~~~~~~~~~~
#
#  For a given .tla, the macro ext/rule/tla is called with 4 parameters:
#  - $1 is the phony radix of the target.
#  - $2 is the real path to the generated file.
#  - $3 are the sources, relative to the srcdir.
#  - $4 is an optional argument to pass "namespaces" (pic for pic .so's).
#  it generates the rules needed to build source files with this extension.
#
#  For a given .tla the macro ext/gen/tla is called with a list of sources and
#  returns a list of intermediate generated files this extension products.
#
#
# $(eval $(call fun/foreach-ext-rule,<PHONY>,<TARGET>,<SOURCES>,[<NS>]))
#
define fun/common-depends
$2: $(1D)/Makefile $(var/toolsdir)/*.mk $(var/toolsdir)/_local_targets.sh
$2: $(var/cfgdir)/*.mk $(var/cfgdir)/cflags.sh
$2: | $(foreach s,$3,$($s_DEPENDS)) $($(1DV)_DEPENDS)
endef

var/exts = $(patsubst ext/rule/%,%,$(filter ext/rule/%,$(.VARIABLES)))
define fun/foreach-ext-rule-nogen
$$(foreach e,$(var/exts),$$(if $$(filter %.$$e,$3),$$(eval $$(call ext/rule/$$e,$1,$2,$$(filter %.$$e,$3),$4,$5))))
$2: | $3
$(eval $(call fun/common-depends,$1,$2,$1))
endef

define fun/foreach-ext-rule
$(call fun/foreach-ext-rule-nogen,$1,$2,$3,$4,$5)
$(if $($1_NOGENERATED),,$2: | _generated)
endef

include $(var/toolsdir)/rules-backend.mk
include $(var/toolsdir)/rules-frontend.mk
include $(var/toolsdir)/rules-misc.mk
include $(var/cfgdir)/rules.mk

ifeq (0,$(MAKELEVEL))
-include $!deps.mk
else
ifneq (,$(filter clean distclean __%,$(MAKECMDGOALS)))
-include $!deps.mk
else
include $!deps.mk
endif
endif

doc:
all full check fast-check www-check clean distclean www selenium fast-selenium pylint::
FORCE: ;
.PHONY: all full check fast-check www-check clean distclean doc www selenium fast-selenium pylint FORCE

var/sourcesvars = $(filter %_SOURCES,$(.VARIABLES))
var/sources    = $(sort $(foreach v,$(var/sourcevars),$($v)))
var/cleanfiles = $(sort $(foreach v,$(filter %_CLEANFILES,$(.VARIABLES)),$($v)))
var/generated  = $(sort $(foreach f,$(filter ext/gen/%,$(.VARIABLES)),$(foreach s,$(var/sourcesvars),$(call $f,$($s),$(s:%_SOURCES=%)))))

var/staticlibs = $(foreach v,$(filter %_LIBRARIES,$(filter-out %_SHARED_LIBRARIES,$(.VARIABLES))),$($v))
var/sharedlibs = $(foreach v,$(filter %_SHARED_LIBRARIES,$(.VARIABLES)),$($v))
var/programs   = $(foreach v,$(filter %_PROGRAMS,$(.VARIABLES)),$($v))
var/datas      = $(foreach v,$(filter %_DATAS,$(.VARIABLES)),$($v))
var/docs       = $(foreach v,$(filter %_DOCS,$(.VARIABLES)),$($v))
var/jars       = $(foreach v,$(filter %_JARS,$(.VARIABLES)),$($v))
var/css        = $(foreach v,$(filter %_CSS,$(.VARIABLES)),$($v))
var/js         = $(foreach v,$(filter %_JS,$(.VARIABLES)),$($v))
var/wwwmodules = $(foreach v,$(filter %_WWWMODULES,$(.VARIABLES)),$($v))

ifeq ($(realpath $(firstword $(MAKEFILE_LIST))),$!Makefile)
##########################################################################
# {{{ Inside the build system
clean::
	find $~ -maxdepth 1 -type f \! -name Makefile \! -name vars.mk -print0 | xargs -0 $(RM)
	$(call fun/expand-if2,$(RM),$(filter-out %/,$(_CLEANFILES)))
	$(call fun/expand-if2,$(RM) -r,$(filter %/,$(_CLEANFILES)))
distclean::
	$(msg/rm) "generated files"
	$(call fun/expand-if2,$(RM),$(var/generated))
	$(call fun/expand-if2,$(RM),$(filter-out %/,$(var/cleanfiles)))
	$(call fun/expand-if2,$(RM) -r,$(filter %/,$(var/cleanfiles)))
	$(RM) $(filter-out %/,$(DISTCLEANFILES))
	$(RM) -r $(filter %/,$(DISTCLEANFILES))
	$(msg/rm) "copied targets"
	$(call fun/expand-if2,$(RM),$(var/docs))
	$(call fun/expand-if2,$(RM),$(var/css))
	$(call fun/expand-if2,$(RM),$(var/jars))
	$(call fun/expand-if2,$(RM),$(var/datas))
	$(call fun/expand-if2,$(RM),$(var/programs:=$(EXEEXT)))
	$(call fun/expand-if2,$(RM),$(var/sharedlibs:=.so*))
	$(call fun/expand-if2,$(RM),$(var/staticlibs:=.a) $(var/staticlibs:=.wa))
	$(msg/rm) "build system"
	$(RM) -r $~
check:: all
	$(var/toolsdir)/_run_checks.sh .
fast-check:: all
	Z_MODE=fast Z_TAG_SKIP='upgrade slow perf' $(var/toolsdir)/_run_checks.sh .
www-check:: | _generated_hdr
	Z_LIST_SKIP="C behave" $(var/toolsdir)/_run_checks.sh .
selenium:: all
	Z_LIST_SKIP="C web" Z_TAG_SKIP='wip' BEHAVE_FLAGS='--tags=web' $(var/toolsdir)/_run_checks.sh .
fast-selenium:: all
	Z_LIST_SKIP="C web" Z_TAG_SKIP='wip upgrade slow' BEHAVE_FLAGS='--tags=web' $(var/toolsdir)/_run_checks.sh .
%.pylint:: %.py
	$(msg/CHECK.py) $<
	pylint $<
pylint:: $(addsuffix lint,$(shell git ls-files '*.py' '**/*.py'))

tags: $(filter-out %.blk.c %.blkk.cc %.min.js,$(var/generated))
syntastic:
ale:
jshint:
.PHONY: tags cscope jshint syntastic ale

define fun/subdirs-targets
$(foreach d,$1,
$(patsubst ./%,%,$(dir $(d:/=)))all::       $(d)all
$(patsubst ./%,%,$(dir $(d:/=)))doc:        $(d)doc
$(patsubst ./%,%,$(dir $(d:/=)))www::       $(d)www
$(patsubst ./%,%,$(dir $(d:/=)))full::      $(d)full
$(patsubst ./%,%,$(dir $(d:/=)))clean::     $(d)clean
$(patsubst ./%,%,$(dir $(d:/=)))pylint::    $(d)pylint
$(d)all::
$(d)doc:
$(d)www::
$(d)full:: $(d)all $(d)www
$(d)check:: $(d)all
	$(var/toolsdir)/_run_checks.sh $(d)
$(d)www-check:: | _generated_hdr
	Z_LIST_SKIP="C behave" $(var/toolsdir)/_run_checks.sh $(d)
$(d)selenium:: $(d)all
	Z_LIST_SKIP="C web" Z_TAG_SKIP='wip' BEHAVE_FLAGS='--tags=web' $(var/toolsdir)/_run_checks.sh $(d)
$(d)fast-selenium:: $(d)all
	Z_LIST_SKIP="C web" Z_TAG_SKIP='wip upgrade slow' BEHAVE_FLAGS='--tags=web' $(var/toolsdir)/_run_checks.sh $(d)
$(d)fast-check:: $(d)all
	Z_MODE=fast Z_TAG_SKIP='upgrade slow perf' $(var/toolsdir)/_run_checks.sh $(d)
$(d)clean::
	find $~$(d) -type f \! -name vars.mk -print0 | xargs -0 $(RM)
	$(call fun/expand-if2,$(RM),$(filter-out %/,$($(d)_CLEANFILES)))
	$(call fun/expand-if2,$(RM) -r,$(filter %/,$($(d)_CLEANFILES)))
$(d)distclean:: distclean
%.pylint:: %.py
	$(msg/CHECK.py) $$<
	pylint $$<
$(d)pylint:: $$(addprefix $(d),$$(addsuffix lint,$$(shell cd '$(d)' && git ls-files '*.py' '**/*.py')))
)
endef
$(eval $(call fun/subdirs-targets,$(patsubst $/%Makefile,%,$(var/makefiles))))

$(foreach p,$(var/staticlibs),$(eval $(call rule/staticlib,$p)))
$(foreach p,$(var/sharedlibs),$(eval $(call rule/sharedlib,$p)))
$(foreach p,$(var/programs),$(eval $(call rule/program,$p)))
$(foreach p,$(var/datas),$(eval $(call rule/datas,$p)))
$(foreach p,$(var/docs),$(eval $(call rule/docs,$p)))
$(foreach p,$(var/jars),$(eval $(call rule/jars,$p)))
$(foreach p,$(var/css),$(eval $(call rule/css,$p)))
$(foreach p,$(var/js),$(eval $(call rule/js,$p)))
$(foreach p,$(var/wwwmodules),$(eval $(call rule/wwwmodule,$p)))

# }}}
else
##########################################################################
# {{{ Only at toplevel

__setup_buildsys_trampoline:
__setup_buildsys_doc:
.PHONY: __setup_buildsys_trampoline __setup_buildsys_doc

ifeq (0,$(MAKELEVEL))

$!deps.mk: $/configure $(var/toolsdir)/configure.inc.sh
	mkdir -p $(@D)
	$< -p $(var/profile) -o $@ || ($(RM) $@; $(msg/alert) 'configure failed')

__setup_buildsys_trampoline: $!deps.mk
	$(msg/echo) 'checking build-system ...'
	$(msg/echo) 'make: Entering directory `$(var/builddir)'"'"
	$(MAKEPARALLEL) __setup_buildsys
	$(msg/echo) 'make: Entering directory `$(var/srcdir)'"'"

toplevel:
.PHONY: toplevel

all full:: toplevel
all full check fast-check www-check clean distclean www selenium fast-selenium:: | __setup_buildsys_trampoline
	$(MAKEPARALLEL) -C $/ -f $!Makefile $(patsubst $/%,%,$(CURDIR)/)$@

__setup_buildsys_doc: | __setup_buildsys_trampoline
	$(MAKEPARALLEL) -C $/ -f $!Makefile $(patsubst $/%,%,$(CURDIR)/)doc

doc: | __setup_buildsys_doc

REPORT_DIR=coverage

www-coverage::
	$(msg/rm) "$!"
	$(RM) -rf $!*
	$(msg/generate) "$!${REPORT_DIR}"
	mkdir -p $!${REPORT_DIR}
	$(msg/generate) "$!"
	cp -a $/* $!
	find -type d -name "javascript" -not -path '*/\.*' -not -path '*/cache/*' | while read line ; do \
		$(msg/generate) "instrumented directory: $!$${line}"; \
		$(RM) -rf $!$${line}; \
		rand=`date | md5sum | head -c 9` && istanbul instrument --save-baseline --baseline-file $!${REPORT_DIR}/coverage-$${rand}-baseline.json --complete-copy --no-compact -x "**/ext/**" --output $!$${line} $/$${line} ; \
	done
	find -type d -name "ext" -not -path '*/\.*' -not -path '*/cache/*' -path '*/javascript/*' | while read line ; do \
		$(msg/generate) "external lib $!$${line}"; \
		cp -r $/$${line} $!$${line}; \
	done
	server=`find -not -path '*/\.*' -name coverage-server.js` ; node $$server $!${REPORT_DIR} &
	MAKELEVEL=0 $(MAKE) P=$(var/profile) NOCHECK=1 Z_LIST_SKIP='C' Z_TAG_SKIP='wip upgrade perf' BEHAVE_FLAGS='--tags=web' L=1 check
	pkill -f coverage-server
	$(msg/generate) "report in $!${REPORT_DIR}"
	istanbul report --root $!${REPORT_DIR} --dir $!${REPORT_DIR}

__setup_buildsys_tags: | __setup_buildsys_trampoline
	$(MAKEPARALLEL) -C $/ -f $!Makefile tags
	@$(if $(shell which ctags),,$(error "Please install ctags: apt-get install exuberant-ctags"))
	cd $/ && ctags $(TAGSOPTION) -o $(TAGSOUTPUT) --recurse=yes --totals=yes --links=no \
	    --c-kinds=+p --c++-kinds=+p --fields=+liaS --extra=+q \
	    -I 'qv_t qm_t qh_t IOP_RPC_IMPL IOP_RPC_CB qvector_t qhp_min_t qhp_max_t MODULE_BEGIN MODULE_END MODULE_DEPENDS_ON+ MODULE_NEEDED_BY+' \
	    --langmap=c:+.blk --langmap=c:+.h --langmap=c++:+.blkk \
	    --regex-c='/^OBJ_CLASS(_NO_TYPEDEF)?\(+([^,]+),/\2_t/o, cclass/' \
	    --regex-c='/^    .*\(\*+([^\ ]+)\)\([a-zA-Z_]+ /\1/x, cmethod/' \
	    --regex-c='/^qvector_t\(+([a-zA-Z_]+)\,/\1/t, qvector/' \
	    --regex-c='/^q[hm]_k.*_t\(+([a-zA-Z_]+)/\1/t, qhash/' \
	    --regex-c='/^MODULE_BEGIN\(+([a-zA-Z_]+)/\1/m, module/' \
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
	    --exclude=".build*" --exclude="Build" --exclude="Config" \
	    --exclude=".git" --exclude=".svn" --exclude="CVS" \
	    --exclude="old" --exclude="new" --exclude="ogu" --exclude="xxx" \
	    --exclude="*.exe" --exclude="*.js" --exclude="*.blk.c" --exclude="*.blkk.cc" \
	    --exclude="*.swf" --exclude="*.ini" --exclude="*fake.c" --exclude="compat.c" \
	    --exclude="js/v8"

tags: TAGSOPTION=
tags: TAGSOUTPUT=.tags
tags: | __setup_buildsys_tags

etags: TAGSOPTION=-e
etags: TAGSOUTPUT=TAGS
etags: | __setup_buildsys_tags

cscope: | __setup_buildsys_trampoline
	cd $/ && \
		find . -regextype sed \
		-regex ".*/[^.]*\.\(c\|cpp\|blk\|blkk\|h\|inc\.c\)$$" \
		-type f -print \
		| grep -v "iop-compat.h" \
		| grep -v "\/compat\/" \
		| sort > .cscope.files && \
		cscope -I$/lib-common/compat -I$/ -ub -i.cscope.files -f.cscope.out

jshint: | __setup_buildsys_trampoline _npm_tools
	$(MAKEPARALLEL) -C $/ -f $!Makefile jshint
	$(msg/CHECK.js)
	git ls-files -- '*.js' | grep -v 'node_modules' | xargs $(var/wwwtool)jshint

www:: $(if $(NOCHECK)$(NOJSHINT),,jshint)

pylint:: | __setup_buildsys_trampoline
	@$(if $(shell which pylint),,$(error "Please install pylint: pip install pylint"))
	$(MAKEPARALLEL) -C $/ -f $!Makefile $(patsubst $/%,%,$(CURDIR)/)pylint

syntastic: | __setup_buildsys_trampoline
	echo '$(CLANGFLAGS)   $(libxml2_CFLAGS) $(openssl_CFLAGS) $(jni_CFLAGS) $(python2_CFLAGS)' | tr -s ' ' '\n' | sed -e '/\"/d' > $/.syntastic_c_config
	echo '$(CLANGXXFLAGS) $(libxml2_CFLAGS) $(openssl_CFLAGS) $(jni_CFLAGS) $(python2_CFLAGS)' | tr -s ' ' '\n' | sed -e '/\"/d' > $/.syntastic_cpp_config

# To use ALE, you will need to add the local_vimrc plugin, and this configuration:
#
# let g:ale_linters = { 'c': ['clang'] }
# let g:local_vimrc = '.local_vimrc.vim'
# call lh#local_vimrc#munge('whitelist', $HOME)
# call lh#local_vimrc#filter_list('asklist', 'v:val != $HOME')
#
ale: | __setup_buildsys_trampoline
	echo "let g:ale_c_clang_options = '" > $/.local_vimrc.vim
	echo ' $(CLANGFLAGS) $(libxml2_CFLAGS) $(openssl_CFLAGS) $(jni_CFLAGS) $(python2_CFLAGS)' | sed -e 's/^/    \\ /g' >> $/.local_vimrc.vim
	echo "\\'" >> $/.local_vimrc.vim

ignore:
	$(foreach v,$(CLEANFILES:/=),grep -q '^/$v$$' .gitignore || echo '/$v' >> .gitignore;)
	$(foreach v,$(var/generated),grep -q '^/$v$$' .gitignore || echo '/$v' >> .gitignore;)
	$(foreach v,$(var/datas),grep -q '^/$v$$' .gitignore || echo '/$v' >> .gitignore;)
	$(foreach v,$(var/docs),grep -q '^/$v$$' .gitignore || echo '/$v' >> .gitignore;)
	$(foreach v,$(var/programs:=$(EXEEXT)),grep -q '^/$v$$' .gitignore || echo '/$v' >> .gitignore;)
	$(foreach v,$(var/sharedlibs:=.so),grep -q '^/$v[*]$$' .gitignore || echo '/$v*' >> .gitignore;)

watch:
	MAKELEVEL= $(var/toolsdir)/_watch.sh $(var/srcdir) ./$(CURDIR:$(var/srcdir)/%=%) $(var/profile) $/$~

check-untracked:
	check-for-untracked-files.sh

translations: | __setup_buildsys_trampoline
	$(MAKEPARALLEL) -C $/ -f $!Makefile translations
	$(MAKEPARALLEL) -C $/www/po xgettext
	$(MAKEPARALLEL) -C $/www/po merge

check-translations: translations
	git status --porcelain $/www/po
	test -z "$(shell git status --porcelain $/www/po)"

www:: $(if $(NOCHECK),,check-translations)

endif
_npm_tools: $/node_modules/build.lock
_generated_hdr:
_generated: _generated_hdr
	$(msg/echo) ' ... generating sources done'
.PHONY: _generated_hdr _generated check-untracked translations check-translations _npm_tools

# }}}
##########################################################################
# {{{ target exports from the build system
ifeq (,$(findstring p,$(MAKEFLAGS)))

$(sort $(var/generated) $(var/datas)) \
$(var/docs)                   \
$(var/programs:=$(EXEEXT))    \
$(var/sharedlibs:=.so)        \
$(var/staticlibs:=.a)         \
$(var/staticlibs:=.pic.a)     \
$(var/staticlibs:=.wa)        \
$(var/staticlibs:=.pic.wa)    \
: FORCE | __setup_buildsys_trampoline
	$(msg/echo) 'building `$@'\'' ...'
	$(MAKEPARALLEL) -C $/ -f $!Makefile $(patsubst $/%,%,$(CURDIR)/)$@

endif
# }}}
##########################################################################
# {{{ __setup_buildsys
#
#   This target uses costly things so we hide it most of the time
ifeq (__setup_buildsys,$(MAKECMDGOALS))

tmp/makefiles := $(shell find "$(var/srcdir)" -name Makefile -type f \( -path '*/.*' -prune -o -path '*/old/*' -prune -o -print \) | while read file; do \
                         grep -q 'include.*base.mk' $$file && echo $$file; done)
tmp/vars      := $(patsubst $(var/srcdir)/%Makefile,$(var/builddir)/%vars.mk,$(tmp/makefiles))

$(tmp/vars): MAKEFLAGS=rsC
$(tmp/vars): $(var/builddir)%/vars.mk: $(var/srcdir)%/Makefile $!deps.mk $(var/toolsdir)/*
	$(msg/generate) $(@R)
	mkdir -p $(@D)
	make --no-print-directory -rsC $(var/srcdir)$* __dump_targets > $@

$(var/builddir)/Makefile: $(var/srcdir)/configure $(tmp/vars) $(var/toolsdir)/*
	$(msg/generate) $(@R)
	mkdir -p $(@D)
	echo 'var/makefiles := $(call fun/msq,$(tmp/makefiles))'      >  $@
	(:$(patsubst $/%Makefile,;echo 'include $~%vars.mk',$(tmp/makefiles)))  >> $@
	echo 'include $(var/toolsdir)/base.mk'                        >> $@

__setup_buildsys: $(var/builddir)/Makefile
.PHONY: __setup_buildsys

endif
# }}}
# {{{ __setup_forward

__setup_forward:
.PHONY: __setup_forward

# }}}
##########################################################################
# {{{ __dump_targets
ifeq (__dump_targets,$(MAKECMDGOALS))

__dump_targets: . = $(patsubst $(var/srcdir)/%,%,$(realpath $(CURDIR))/)
__dump_targets:
	echo 'ifneq (,$$(realpath $.Makefile))'
	$(foreach v,$(filter %_DOCS %_DATAS %_PROGRAMS %_LIBRARIES %_JARS %_CSS %_JS,$(.VARIABLES)),\
	    echo '$v += $(call fun/exportvars,$(CURDIR),$($v))';)
	$(foreach v,$(filter %_WWWMODULES %_WWWSCRIPTS,$(.VARIABLES)),\
	    echo '$.$v += $(call fun/exportvars,$(CURDIR),$($v))';)
	$(foreach v,$(filter %_DEPENDS %_SOURCES %_DESTDIR %_CONFIG %_MINIFY,$(.VARIABLES)),\
	    echo '$.$v += $(call fun/exportvars,$(CURDIR),$($v))';)
	$(foreach v,$(filter %_EXPORT,$(.VARIABLES)),\
		$(foreach vv,$($v),\
			echo '$.$(vv) += $(call fun/exportvars,$(CURDIR),$($(vv)))';))
	$(foreach v,$(filter %LINKER %LIBS %COMPILER %FLAGS %CFLAGSBASE %INCPATH %JSONPATH %CLASSRANGE %IOPVER %_SOVERSION %_NOCHECK %_CLASSPATH %_NOGENERATED,$(filter-out MAKE%,$(.VARIABLES))),\
	    echo '$.$v += $(call fun/msq,$($v))';)
	echo '$._CLEANFILES += $(call fun/msq,$(call fun/rebase,$(CURDIR),$(CLEANFILES)))'
	echo 'DISTCLEANFILES += $(call fun/msq,$(call fun/rebase,$(CURDIR),$(DISTCLEANFILES)))'
	echo ''
	make -nspqr | $(var/toolsdir)/_local_targets.sh \
	    "$(var/srcdir)" "$." "$(var/toolsdir)" "$(var/cfgdir)" | \
	    sed -n -e 's~$$~ FORCE ; $$(MAKE) -w$(if $.,C $.) $$(subst $.,,$$@)~p'
	echo 'endif'

.PHONY: __dump_targets

endif
#}}}
endif
