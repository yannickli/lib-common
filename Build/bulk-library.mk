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

define fun/forward-real
$1: | __setup_buildsys_trampoline
	$(msg/echo) 'building `$$@'\'' ...'
	$(MAKEPARALLEL) -C $/ -f $!Makefile $$(patsubst $/%,%,$$(CURDIR)/)$$@
.PHONY: $1
endef

define fun/forward
$$(foreach g,$1,$$(eval $$(call fun/do-once,forward-real-$$g,$$(call fun/forward-real,$$g))))
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
#[ easy ones (ld,a,wa,...) ]##########################################{{{#

define ext/rule/ld
$2: $3
endef

define ext/rule/a
$2: $$(patsubst %.a,$~%$4.a,$3)
endef

define ext/rule/wa
$2: $$(patsubst %.wa,$~%$4.wa,$3)
endef

#}}}
#[ .c files ]#########################################################{{{#

define ext/rule/c
tmp/$2/ns   := $$(if $$($(1D)/_CFLAGS)$$($1_CFLAGS),.$(2F))
tmp/$2/objs := $$(patsubst %.c,$~%$$(tmp/$2/ns)$4.o,$3)

$2: $$(tmp/$2/objs)

$$(tmp/$2/objs): $~%$$(tmp/$2/ns)$4.o: %.c $(var/toolsdir)/* $!$(1D)/vars.mk
	$(msg/COMPILE.c) $$(<R)
	$(CC) $(CFLAGS) $$($(1D)/_CFLAGS) $$($1_CFLAGS) $$($$*.c_CFLAGS) \
	    -MP -MMD -MQ $$@ -MF $$(@:o=dep) \
	    $$(if $$(findstring .pic,$4),-fPIC) -g -c -o $$@ $$<

-include $$(tmp/$2/objs:o=dep)
endef

#}}}
#[ lex ]##############################################################{{{#

define ext/gen/l
$(call fun/forward,$(1:l=c))
endef

define fun/expand-l
$(3:l=c): %.c: %.l
	$(msg/COMPILE.l) $$(@R)
	flex -R -o $$@ $$<
	sed -i -e 's/^extern int isatty.*;//' \
	       -e 's/^\t\tint n; \\/		size_t n; \\/' $$@

.PRECIOUS: $(3:l=c)
__generate_files: $(3:l=c)
distclean::
	$(msg/rm) $(1D) lexers
	$(RM) $(3:l=c)
endef

define ext/rule/l
$$(foreach t,$3,$$(eval $$(call fun/do-once,$$t,$$(call fun/expand-l,$1,$2,$$t,$4))))
$$(eval $$(call ext/rule/c,$1,$2,$(3:l=c),$4))
$1: | __generate_files
endef

#}}}
#[ tokens ]###########################################################{{{#

define ext/gen/tokens
$(call fun/forward,$(1:.tokens=tokens.h) $(1:.tokens=tokens.c))
endef

define fun/expand-tokens
tmp/$2/toks_h := $$(patsubst %.tokens,%tokens.h,$3)
tmp/$2/toks_c := $$(patsubst %.tokens,%tokens.c,$3)

$$(tmp/$2/toks_h): %tokens.h: %.tokens $(var/toolsdir)/_tokens.sh
	$(msg/generate) $$(@R)
	cd $$(<D) && $(var/toolsdir)/_tokens.sh $$(<F) $$(@F) || ($(RM) $$(@F) && exit 1)

$$(tmp/$2/toks_c): %tokens.c: %.tokens %tokens.h $(var/toolsdir)/_tokens.sh
	$(msg/generate) $$(@R)
	cd $$(<D) && $(var/toolsdir)/_tokens.sh $$(<F) $$(@F) || ($(RM) $$(@F) && exit 1)

.PRECIOUS: $$(tmp/$2/toks_h) $$(tmp/$2/toks_c)
__generate_files: $$(tmp/$2/toks_h) $$(tmp/$2/toks_c)
distclean::
	$(msg/rm) $(1D) tokens
	$(RM) $$(tmp/$2/toks_h) $$(tmp/$2/toks_c)
endef

define ext/rule/tokens
$$(foreach t,$3,$$(eval $$(call fun/do-once,$$t,$$(call fun/expand-tokens,$1,$2,$$t,$4))))
$$(eval $$(call ext/rule/c,$1,$2,$(3:.tokens=tokens.c),$4))
$2 $$(filter %.c,$$($1_SOURCES)): | __generate_files
endef

#}}}
#[ lua ]##############################################################{{{#

define ext/gen/lua
$(call fun/forward,$(1:.lua=.lc.bin))
endef

define fun/expand-lua
$(3:lua=lc.bin): %.lc.bin: %.lua
	$(msg/echo) " LUA" $$(<R)
	luac -o $$*.lc $$<
	util/bldutils/blob2c $$*.lc > $$@ || ($(RM) $$@; exit 1)

.PRECIOUS: $(3:.lua=.lc.bin)
__generate_files: $(3:.lua=.lc.bin)
distclean::
	$(msg/rm) $(1D) lua bytecode
	$(RM) $(3:.lua=.lc) $(3:.lua=.lc.bin)
endef

define ext/rule/lua
$$(foreach t,$3,$$(eval $$(call fun/do-once,$$t,$$(call fun/expand-lua,$1,$2,$$t,$4))))
$2 $$(filter %.c,$$($1_SOURCES)): | __generate_files
endef

#}}}
#[ farchs/fcs ]#######################################################{{{#

define ext/gen/farch
$(call fun/forward,$(1:.farch=farch.h) $(1:.farch=farch.c))
endef

define ext/rule/farch
tmp/$2/farch := $$(patsubst %.farch,%farch,$3)

$$(addsuffix .c,$$(tmp/$2/farch)): %farch.c: %farch.h util/bldutils/buildfarch
$$(addsuffix .h,$$(tmp/$2/farch)): %farch.h: %.farch  util/bldutils/buildfarch
	$(msg/generate) $$(@R)
	cd $$(@D) && $/util/bldutils/buildfarch -r $(1D)/ -d $!$$@.dep -n $$(*F) `cat $$(<F)`

$$(eval $$(call ext/rule/c,$1,$2,$$(tmp/$2/farch:=.c),$4))
.PRECIOUS: $$(tmp/$2/farch:=.h) $$(tmp/$2/farch:=.c)
__generate_files: $$(tmp/$2/farch:=.h) $$(tmp/$2/farch:=.c)
distclean::
	$(msg/rm) $(1D) farchs
	$(RM) $$(tmp/$2/farch:=.h) $$(tmp/$2/farch:=.c)
-include $$(patsubst %,$~%.c.dep,$3)
$2 $$(filter %.c,$$($1_SOURCES)): | __generate_files
endef

# -- fcs

define ext/gen/fc
$(call fun/forward,$(1:=.c))
endef

define ext/rule/fc
$$(addsuffix .c,$3): %.fc.c: %.fc util/bldutils/farchc
	$(msg/generate) $$(@R)
	$/util/bldutils/farchc -d $~$$@.dep -o $$@ $$<

.PRECIOUS: $(3:=.c)
__generate_files: $(3:=.c)
distclean::
	$(msg/rm) $(1D) fc
	$(RM) $(3:=.c)
-include $$(patsubst %,$~%.c.dep,$3)
$2 $$(filter %.c,$$($1_SOURCES)): | __generate_files
endef

#}}}

#
# $(eval $(call fun/foreach-ext-rule,<PHONY>,<TARGET>,<SOURCES>,[<NS>]))
#
var/exts := $(patsubst ext/rule/%,%,$(filter ext/rule/%,$(.VARIABLES)))
define fun/foreach-ext-rule
$2: | $$($1_SOURCES) $$($1_DEPENDS)
$$(foreach e,$(var/exts),$$(if $$(filter %.$$e,$3),$$(eval $$(call ext/rule/$$e,$1,$2,$$(filter %.$$e,$3),$4))))
endef

var/exts-gen := $(patsubst ext/gen/%,%,$(filter ext/gen/%,$(.VARIABLES)))
define fun/foreach-ext-gen
$$(foreach e,$(var/exts-gen),$$(if $$(filter %.$$e,$1),$$(eval $$(call ext/gen/$$e,$$(filter %.$$e,$1)))))
endef

#
# main targets (entry points)
#
#[ _LIBRARIES ]#######################################################{{{#

define goal/staticlib
$(call fun/forward,$1.a $1.wa)
$$(eval $$(call fun/foreach-ext-gen,$$($1_SOURCES)))
endef

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

define goal/sharedlib
$(call fun/forward,$1.so)
$$(eval $$(call fun/foreach-ext-gen,$$($1_SOURCES)))
endef

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
	    $(LDFLAGS) $$($(1D)_LDFLAGS) $$($1_LDFLAGS) \
	    -Wl,--whole-archive $$(filter %.wa,$$^) \
	    -Wl,--no-whole-archive $$(filter %.a,$$^) $$($1_LIBS) \
	    -Wl,-soname,$(1F).so$$(tmp/$1/sover)
	$$(if $$(tmp/$1/build),ln -sf $/$$@ $~$1.so)

$(1D)/clean::
	$(RM) $1.so* $/$1.so*
endef

#}}}
#[ _PROGRAMS ]########################################################{{{#

define goal/program
$(call fun/forward,$1$(EXEEXT))
$$(eval $$(call fun/foreach-ext-gen,$$($1_SOURCES)))
endef

define rule/program
$(1D)/all:: $1$(EXEEXT)
$1$(EXEEXT): $~$1.exe FORCE
	$(FASTCP) $$< $$@

$$(eval $$(call fun/foreach-ext-rule,$1,$~$1.exe,$$($1_SOURCES)))
$~$1.exe:
	$(msg/LINK.c) $$(@R)
	$(CC) $(CFLAGS) $$($(1D)/_CFLAGS) $$($1_CFLAGS) \
	    -o $$@ $$(filter %.o %.ld,$$^) \
	    $(LDFLAGS) $$($(1D)_LDFLAGS) $$($1_LDFLAGS) \
	    -Wl,--whole-archive $$(filter %.wa,$$^) \
	    -Wl,--no-whole-archive $$(filter %.a,$$^) $$($1_LIBS)
$(1D)/clean::
	$(RM) $1$(EXEEXT) $/$1$(EXEEXT)
endef
#}}}
