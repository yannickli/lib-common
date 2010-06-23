##########################################################################
#                                                                        #
#  Copyright (C) 2004-2010 INTERSEC SAS                                  #
#                                                                        #
#  Should you receive a copy of this source code, you must check you     #
#  have a proper, written authorization of INTERSEC to hold it. If you   #
#  don't have such an authorization, you must DELETE all source code     #
#  files in your possession, and inform INTERSEC of the fact you obtain  #
#  these files. Should you not comply to these terms, you can be         #
#  prosecuted in the extent permitted by applicable law.                 #
#                                                                        #
##########################################################################

define fun/common-depends
$2: $(1D)/Makefile $(var/toolsdir)/* $(var/cfgdir)/*.mk
$2: $$(foreach s,$3,$$($$s_DEPENDS)) | $$($(1D)/_DEPENDS)
endef

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
######################################################################{{{#

define ext/rule/ld
$2: $3
endef

define ext/rule/a
$2: $$(patsubst %.a,$~%$4.a,$3)
endef

define ext/rule/wa
$2: $$(patsubst %.wa,$~%$4.wa,$3)
endef

#[ c ]####################################################################

define ext/expand/c
$3: $~%$$(tmp/$2/ns)$4.o: %.c | __$(1D)_generated
	mkdir -p $$(@D)
	$(msg/COMPILE.c) $$(<R)
	$(CC) $(CFLAGS) $$($(1D)/_CFLAGS) $$($1_CFLAGS) $$($$*.c_CFLAGS) \
	    -MP -MMD -MT $$@ -MF $$(@:o=dep) \
	    $$(if $$(findstring .pic,$4),-fPIC) -g -c -o $$@ $$<
-include $(3:o=dep)
endef

define ext/rule/c
tmp/$2/ns   := $$(if $$($(1D)/_CFLAGS)$$($1_CFLAGS),.$(2F)).$(call fun/path-mangle,$(1D))
tmp/$2/objs := $$(patsubst %.c,$~%$$(tmp/$2/ns)$4.o,$3)
$2: $$(tmp/$2/objs)
$$(foreach o,$$(tmp/$2/objs),$$(eval $$(call fun/do-once,ext/expand/c/$$o,$$(call ext/expand/c,$1,$2,$$o,$4))))
$$(eval $$(call fun/common-depends,$1,$$(tmp/$2/objs),$3))
endef

#[ lex ]##################################################################

ext/gen/l = $(call fun/patsubst-filt,%.l,%.c,$1)

define ext/expand/l
$(3:l=c): %.c: %.l
	$(msg/COMPILE.l) $$(@R)
	$(RM) $$@
	flex -R -o $$@ $$<
	sed -i -e 's/^extern int isatty.*;//' \
	       -e 's/^\t\tint n; \\/		size_t n; \\/' $$@
	chmod a-w $$@
__$(1D)_generated: $(3:l=c)
$$(eval $$(call fun/common-depends,$1,$(3:l=c),$3))
endef

define ext/rule/l
$$(foreach t,$3,$$(eval $$(call fun/do-once,$$t,$$(call ext/expand/l,$1,$2,$$t,$4))))
$$(eval $$(call ext/rule/c,$1,$2,$(3:l=c),$4))
endef

#}}}
-include $(var/cfgdir)/rules.mk

#
# $(eval $(call fun/foreach-ext-rule,<PHONY>,<TARGET>,<SOURCES>,[<NS>]))
#
var/exts = $(patsubst ext/rule/%,%,$(filter ext/rule/%,$(.VARIABLES)))
define fun/foreach-ext-rule
$$(foreach e,$(var/exts),$$(if $$(filter %.$$e,$3),$$(eval $$(call ext/rule/$$e,$1,$2,$$(filter %.$$e,$3),$4))))
$2: | $$($1_SOURCES) __$(1D)_generated
$$(eval $$(call fun/common-depends,$1,$2,$1))
__$(1D)_generated:
.PHONY: __$(1D)_generated
endef

#
# main targets (entry points)
#
#[ _LIBRARIES ]#######################################################{{{#

define rule/staticlib
$~$1.wa: $~$1.a
	$(FASTCP) $$< $$@
$1.a:  $~$1.a
$1.wa: $~$1.wa
$~$1.pic.wa: $~$1.pic.a
	$(FASTCP) $$< $$@
$1.pic.a:  $~$1.pic.a
$1.pic.wa: $~$1.pic.wa
.PHONY: $1.a $1.wa $1.pic.a $1.pic.wa

$(1D)/all:: $~$1.a
$$(eval $$(call fun/foreach-ext-rule,$1,$~$1.a,$$($1_SOURCES)))
$~$1.a:
	$(msg/LINK.a) $$(@R)
	$(RM) $$@
	$(AR) crs $$@ $$(filter %.o,$$^)

$$(eval $$(call fun/foreach-ext-rule,$1,$~$1.pic.a,$$($1_SOURCES),.pic))
$~$1.pic.a:
	$(msg/LINK.a) $$(@R)
	$(RM) $$@
	$(AR) crs $$@ $$(filter %.o,$$^)
endef

#}}}
#[ _SHARED_LIBRARIES ]################################################{{{#

define rule/sharedlib
tmp/$1/sover := $$(if $$(word 1,$$($1_SOVERSION)),.$$(word 1,$$($1_SOVERSION)))
tmp/$1/build := $$(tmp/$1/sover)$$(if $$(word 2,$$($1_SOVERSION)),.$$(word 2,$$($1_SOVERSION)))

$(1D)/all:: $1.so
$1.so: $~$1.so$$(tmp/$1/build) FORCE
	$(FASTCP) $$< $/$$@$$(tmp/$1/build)
	$$(if $$(tmp/$1/build),cd $/$$(@D) && ln -sf $$(@F)$$(tmp/$1/build) $$(@F))
	$$(if $$(tmp/$1/sover),cd $/$$(@D) && ln -sf $$(@F)$$(tmp/$1/build) $$(@F)$$(tmp/$1/sover))

$$(eval $$(call fun/foreach-ext-rule,$1,$~$1.so$$(tmp/$1/build),$$($1_SOURCES),.pic))
$~$1.so$$(tmp/$1/build):
	$(msg/LINK.c) $$(@R)
	$(CC) $(CFLAGS) $$($(1D)/_CFLAGS) $$($1_CFLAGS) \
	    -fPIC -shared -o $$@ $$(filter %.o %.ld,$$^) \
	    $(LDFLAGS) $$($(1D)/_LDFLAGS) $$($(1D)_LDFLAGS) $$($1_LDFLAGS) \
	    -Wl,--whole-archive $$(filter %.wa,$$^) \
	    -Wl,--no-whole-archive $$(filter %.a,$$^) \
	    $(LIBS) $$($(1D)/_LIBS) $$($(1D)_LIBS) $$($1_LIBS) \
	    -Wl,-soname,$(1F).so$$(tmp/$1/sover)
	$$(if $$(tmp/$1/build),ln -sf $/$$@ $~$1.so)

$(1D)/clean::
	$(RM) $1.so*
endef

#}}}
#[ _PROGRAMS ]########################################################{{{#

define rule/exe
$1$(EXEEXT): $~$1.exe FORCE
	$(FASTCP) $$< $$@

$$(eval $$(call fun/foreach-ext-rule,$1,$~$1.exe,$$($1_SOURCES),$4))
$~$1.exe:
	$(msg/LINK.c) $$(@R)
	$(CC) $(CFLAGS) $$($(1D)/_CFLAGS) $$($1_CFLAGS) \
	    -o $$@ $$(filter %.o %.ld,$$^) \
	    $(LDFLAGS) $$($(1D)/_LDFLAGS) $$($(1D)_LDFLAGS) $$($1_LDFLAGS) \
	    -Wl,--whole-archive $$(filter %.wa,$$^) \
	    -Wl,--no-whole-archive $$(filter %.a,$$^) \
	    $(LIBS) $$($(1D)/_LIBS) $$($(1D)_LIBS) $$($1_LIBS)
$(1D)/clean::
	$(RM) $1$(EXEEXT)
endef

define rule/program
$(1D)/all:: $1$(EXEEXT)
$$(eval $$(call rule/exe,$1,$2,$3))
$(if $(filter %chk,$1),
$(1D)/check:: $1$(EXEEXT)
	$$/lib-common/scripts/runchk $1
)
endef

#}}}
#[ _DATAS ]###########################################################{{{#

define rule/datas
$(1D)/all:: $1
$$(eval $$(call fun/foreach-ext-rule,$1,$1,$$($1_SOURCES)))
endef

#}}}
