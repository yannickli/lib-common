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

define fun/common-depends
$2: $(1D)/Makefile $(var/toolsdir)/*.mk $(var/toolsdir)/_local_targets.sh
$2: $(var/cfgdir)/*.mk $(var/cfgdir)/cflags.sh
$2: $(foreach s,$3,$($s_DEPENDS)) | $($(1DV)_DEPENDS)
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
#[ ld,a,wa ]##########################################################{{{#

define ext/rule/ld
$2: $3
endef

define ext/rule/a
$2: $(patsubst %.a,$~%$4.a,$3)
endef

define ext/rule/wa
$2: $(patsubst %.wa,$~%$4.wa,$3)
endef

#}}}
#[ c ]################################################################{{{#

# ext/expand/c <PHONY>,<TARGET>,<C>,<NS>,<OBJ>
define ext/expand/c
$5: NOCHECK_=$$(NOCHECK)$(findstring clang,$(CC_BASE))$($(1DV)_NOCHECK)$($1_NOCKECK)$$($3_NOCHECK)$(findstring .iop.c,$3)
$5: FLAGS_=$($(1DV)_CFLAGS) $($1_CFLAGS) $($3_CFLAGS)
$5: CLANGFLAGS_=$($(1DV)_CLANGFLAGS) $($1_CLANGFLAGS) $($3_CLANGFLAGS) $$(CLANGFLAGS)
$5: $3 | _generated
	mkdir -p $$(@D)
	$$(if $$(NOCHECK_),,$(msg/CHECK.c) $3)
	$$(if $$(NOCHECK_),,clang $$(CLANGFLAGS_) $$(filter-out -D_FORTIFY_SOURCE=%,$$(FLAGS_)) \
	    -x c -O0 -fsyntax-only -D_FORTIFY_SOURCE=0 -o /dev/null $3)
	$(msg/COMPILE.c) $3
	$(CC) $(if $(filter %.c,$3),,-x c) -g $(CFLAGS) $$(FLAGS_) -MP -MMD -MT $5 -MF $5.d \
	    $(if $(findstring .pic,$4),-fPIC -DSHARED,$(CNOPICFLAGS)) -c -o $5 $3
-include $5.d
endef

define ext/rule/c
tmp/$2/ns   := $(if $($(1DV)_CFLAGS)$($1_CFLAGS),.$(2F)).$(call fun/path-mangle,$(1D))$4
tmp/$2/objs := $$(patsubst %,$~%$$(tmp/$2/ns)$(OBJECTEXT).o,$3)
$2: $$(tmp/$2/objs)
$$(foreach c,$3,$$(eval $$(call fun/do-once,ext/expand/c/$$c$$(tmp/$2/ns),\
    $$(call ext/expand/c,$1,$2,$$c,$4,$~$$c$$(tmp/$2/ns)$(OBJECTEXT).o))))
$$(eval $$(call fun/common-depends,$1,$$(tmp/$2/objs),$3))
endef

#}}}
#[ cc ]###############################################################{{{#

# ext/expand/c <PHONY>,<TARGET>,<C>,<NS>,<OBJ>
define ext/expand/cc
$5: NOCHECK_=$$(NOCHECK)$(findstring clang,$(CXX_BASE))$($(1DV)_NOCHECK)$($1_NOCKECK)$$($3_NOCHECK)
$5: FLAGS_=$($(1DV)_CXXFLAGS) $($1_CXXFLAGS) $($3_CXXFLAGS)
$5: CLANGXXFLAGS_=$($(1DV)_CLANGXXFLAGS) $($1_CLANGXXFLAGS) $($3_CLANGXXFLAGS) $$(CLANGXXFLAGS)
$5: $3 | _generated
	mkdir -p $$(@D)
	$$(if $$(NOCHECK_),,$(msg/CHECK.C) $3)
	$$(if $$(NOCHECK_),,clang++ $$(CLANGXXFLAGS_) $$(filter-out -D_FORTIFY_SOURCE=%,$$(FLAGS_)) \
	    -x c++ -O0 -fsyntax-only -D_FORTIFY_SOURCE=0 -o /dev/null $3)
	$(msg/COMPILE.C) $3
	$(CXX) $(if $(filter %.cc %.cpp,$3),,-x c++) -g $(CXXFLAGS) $$(FLAGS_) -MP -MMD -MT $5 -MF $5.d \
	    $(if $(findstring .pic,$4),-fPIC -DSHARED,$(CXXNOPICFLAGS)) -c -o $5 $3
-include $5.d
endef

define ext/rule/cc
tmp/$2/ns   := $$(if $($(1DV)_CXXFLAGS)$($1_CXXFLAGS),.$(2F)).$(call fun/path-mangle,$(1D))$4
tmp/$2/objs := $$(patsubst %,$~%$$(tmp/$2/ns)$(OBJECTEXT).o,$3)
$2: $$(tmp/$2/objs)
$$(foreach c,$3,$$(eval $$(call fun/do-once,ext/expand/c/$$c$$(tmp/$2/ns),\
    $$(call ext/expand/cc,$1,$2,$$c,$4,$~$$c$$(tmp/$2/ns)$(OBJECTEXT).o))))
$$(eval $$(call fun/common-depends,$1,$$(tmp/$2/objs),$3))
endef

#}}}
#[ h ]################################################################{{{#

define ext/rule/h
$(eval $(call fun/common-depends,$1,$3,$3))
_generated_hdr: $3
endef

#}}}
#[ lex ]##############################################################{{{#

ext/gen/l = $(call fun/patsubst-filt,%.l,%.c,$1)

define ext/expand/l
$(3:l=c): $3
	$(msg/COMPILE.l) $3
	flex -R -o $~$3.c+ $$<
	sed $(if $(filter $(OS),darwin),-i '',-i) -e 's/^extern int isatty.*;//' \
	       -e '1s/^/#if defined __clang__ || ((__GNUC__ << 16) + __GNUC_MINOR__ >= (4 << 16) +2)\n#pragma GCC diagnostic ignored "-Wsign-compare"\n#endif\n/' \
	       -e 's/^\t\tint n; \\/            size_t n; \\/' \
	       -e 's/^int .*get_column.*;//' \
	       -e 's/^void .*set_column.*;//' \
	       -e 's!$~$3.c+"!$(3:l=c)"!g' \
	       $~$3.c+
	$(MV) $~$3.c+ $$@ && chmod a-w $$@
endef

define ext/rule/l
$$(foreach t,$3,$$(eval $$(call fun/do-once,$$t,$$(call ext/expand/l,$1,$2,$$t,$4))))
$(eval $(call ext/rule/c,$1,$2,$(3:l=c),$4))
$(eval $(call fun/common-depends,$1,$(3:l=c),$3))
_generated: $(3:l=c)
endef

#}}}
#[ web ]###############################################################{{{#
# {{{ css

ext/gen/css = $(if $(filter %.css,$1),$(strip $($2_DESTDIR))/$(notdir $(1:css=min.css)))

define ext/expand/css
$(strip $($1_DESTDIR))/$(notdir $(3:css=min.css)): $3
	$(msg/MINIFY.css) $3
	mkdir -p `dirname "$~$$@"`
	lessc -M $$< $$@ > $~$$@.d
	(cat $(var/cfgdir)/head.css && lessc --compress $$<) > $$@+
	$(MV) $$@+ $$@ && chmod a-w $$@
-include $~$(strip $($1_DESTDIR))/$(notdir $(3:css=min.css)).d
$2: $(strip $($1_DESTDIR))/$(notdir $(3:css=min.css))
endef

define ext/rule/css
$$(foreach t,$3,$$(eval $$(call fun/do-once,$$t,$$(call ext/expand/css,$1,$2,$$t,$4))))
$(eval $(call fun/common-depends,$1,$(strip $($1_DESTDIR))/$(notdir $(3:css=min.css)),$3))
endef

# }}}
# {{{ less

ext/gen/less = $(if $(filter %.less,$1),$(strip $($2_DESTDIR))/$(notdir $(1:less=css)) $(call ext/gen/css,$(strip $($2_DESTDIR))/$(notdir $(1:less=css)),$2))

define ext/expand/less
$(strip $($1_DESTDIR))/$(notdir $(3:less=css)): $3
	$(msg/COMPILE.less) $3
	mkdir -p `dirname "$~$$@"`
	lessc -M $$< $$@ > $~$$@.d
	lessc $$< $$@+
	$(MV) $$@+ $$@ && chmod a-w $$@
-include $~$(strip $($1_DESTDIR))/$(notdir $(3:less=css)).d
$2: $(strip $($1_DESTDIR))/$(notdir $(3:less=css))
endef

define ext/rule/less
$$(foreach t,$3,$$(eval $$(call fun/do-once,$$t,$$(call ext/expand/less,$1,$2,$$t,$4))))
$(eval $(call ext/rule/css,$1,$2,$(strip $($1_DESTDIR))/$(notdir $(3:less=css)),$4))
$(eval $(call fun/common-depends,$1,$(strip $($1_DESTDIR))/$(notdir $(3:less=css)),$3))
endef

# }}}
# {{{ uglifyjs

ext/gen/js = $(call fun/patsubst-filt,%.js,%.min.js,$1)

define ext/expand/js
$(3:js=min.js): $3
	$(msg/MINIFY.js) $3
	(cat $(var/cfgdir)/head.js && uglifyjs $$<) > $$@
$2: $(3:js=min.js)
endef

define ext/rule/js
$$(foreach t,$3,$$(eval $$(call fun/do-once,$$t,$$(call ext/expand/js,$1,$2,$$t,$4))))
$(eval $(call fun/common-depends,$1,$(3:js=min.js),$3))
endef

# }}}
#}}}

-include $(var/cfgdir)/rules.mk

#
# $(eval $(call fun/foreach-ext-rule,<PHONY>,<TARGET>,<SOURCES>,[<NS>]))
#
var/exts = $(patsubst ext/rule/%,%,$(filter ext/rule/%,$(.VARIABLES)))
define fun/foreach-ext-rule-nogen
$$(foreach e,$(var/exts),$$(if $$(filter %.$$e,$3),$$(eval $$(call ext/rule/$$e,$1,$2,$$(filter %.$$e,$3),$4))))
$2: | $($1_SOURCES)
$(eval $(call fun/common-depends,$1,$2,$1))
endef

define fun/foreach-ext-rule
$(call fun/foreach-ext-rule-nogen,$1,$2,$3,$4)
$2: | _generated
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

$(1DV)all:: $~$1.a
$(eval $(call fun/foreach-ext-rule,$1,$~$1.a,$($1_SOURCES)))
$~$1.a:
	$(msg/LINK.a) $$(@R)
	$(RM) $$@+ && $(AR) crs $$@+ $$(filter %.o %.oo,$$^) && $(MV) $$@+ $$@

$(eval $(call fun/foreach-ext-rule,$1,$~$1.pic.a,$($1_SOURCES),.pic))
$~$1.pic.a:
	$(msg/LINK.a) $$(@R)
	$(RM) $$@+ && $(AR) crs $$@+ $$(filter %.o %.oo,$$^) && $(MV) $$@+ $$@
endef

#}}}
#[ _SHARED_LIBRARIES ]################################################{{{#

ifeq ($(OS),darwin)
define rule/sharedlib
tmp/$1/sover := $$(if $$(word 1,$$($1_SOVERSION)),$$(word 1,$$($1_SOVERSION)))
tmp/$1/build := $$(tmp/$1/sover)$$(if $$(word 2,$$($1_SOVERSION)),.$$(word 2,$$($1_SOVERSION)))

$(1DV)all:: $1$(var/sharedlibext)
$1$(var/sharedlibext): $~$1$(var/sharedlibext)$$(tmp/$1/build) FORCE
	$$(if $$(NOLINK),:,chmod a-wx $$< && $(FASTCP) $$< $/$$@$$(tmp/$1/build))
	$$(if $$(tmp/$1/build),cd $/$$(@D) && ln -sf $$(@F)$$(tmp/$1/build) $$(@F))
	$$(if $$(tmp/$1/sover),cd $/$$(@D) && ln -sf $$(@F)$$(tmp/$1/build) $$(@F)$$(tmp/$1/sover))

$$(eval $$(call fun/foreach-ext-rule,$1,$~$1$(var/sharedlibext)$$(tmp/$1/build),$$($1_SOURCES),.pic))
$~$1$(var/sharedlibext)$$(tmp/$1/build): _L=$(or $($1_LINKER),$(CC))
$~$1$(var/sharedlibext)$$(tmp/$1/build):
	$(msg/LINK.c) $$(@R)
	$$(if $$(NOLINK),:,ld -o $$@ -arch x86_64 $$(filter %.o %.oo %.ld %.a,$$^)    \
	    $$(addprefix -force_load ,$$(filter %.wa,$$^))              \
	    -dylib -macosx_version_min 10.8                             \
	    -lc $$(if $$(filter clang++,$$(_L)),-lstdc++)                \
	    $(filter-out -lrt,$(LIBS) $($(1DV)_LIBS) $($(1D)_LIBS) $($1_LIBS)))
	$$(if $$(NOLINK),:,$$(if $$(tmp/$1/build),ln -sf $/$$@ $~$1$(var/sharedlibext)))

$(1DV)clean::
	$(RM) $1$(var/sharedlibext)*
endef
else
define rule/sharedlib
tmp/$1/sover := $$(if $$(word 1,$$($1_SOVERSION)),.$$(word 1,$$($1_SOVERSION)))
tmp/$1/build := $$(tmp/$1/sover)$$(if $$(word 2,$$($1_SOVERSION)),.$$(word 2,$$($1_SOVERSION)))

$(1DV)all:: $1$(var/sharedlibext)
$1$(var/sharedlibext): $~$1$(var/sharedlibext)$$(tmp/$1/build) FORCE
	$$(if $$(NOLINK),:,chmod a-wx $$< && $(FASTCP) $$< $/$$@$$(tmp/$1/build))
	$$(if $$(tmp/$1/build),cd $/$$(@D) && ln -sf $$(@F)$$(tmp/$1/build) $$(@F))
	$$(if $$(tmp/$1/sover),cd $/$$(@D) && ln -sf $$(@F)$$(tmp/$1/build) $$(@F)$$(tmp/$1/sover))

$$(eval $$(call fun/foreach-ext-rule,$1,$~$1$(var/sharedlibext)$$(tmp/$1/build),$$($1_SOURCES),.pic))
$~$1$(var/sharedlibext)$$(tmp/$1/build): _L=$(or $($1_LINKER),$(CC))
$~$1$(var/sharedlibext)$$(tmp/$1/build):
	$(msg/LINK.c) $$(@R)
	$$(if $$(NOLINK),:,$$(_L) $(CFLAGS) $($(1DV)_CFLAGS) $($1_CFLAGS) \
	    -fPIC -shared -o $$@ $$(filter %.o %.oo,$$^) \
	    $$(addprefix -Wl$$(var/comma)--version-script$$(var/comma),$$(filter %.ld,$$^)) \
	    $$(LDFLAGS) $$($(1DV)_LDFLAGS) $$($(1D)_LDFLAGS) $$($1_LDFLAGS) \
	    -Wl,--whole-archive $$(filter %.wa,$$^) \
	    -Wl,--no-whole-archive $$(filter %.a,$$^) \
		$$(if $$(filter clang++,$$(_L)),-lstdc++) \
	    $(LIBS) $($(1DV)_LIBS) $($(1D)_LIBS) $($1_LIBS) \
	    -Wl,-soname,$(1F)$(var/sharedlibext)$$(tmp/$1/sover))
	$$(if $$(NOLINK),:,$$(if $$(tmp/$1/build),ln -sf $/$$@ $~$1$(var/sharedlibext)))

$(1DV)clean::
	$(RM) $1$(var/sharedlibext)*
endef
endif

#}}}
#[ _PROGRAMS ]########################################################{{{#

ifeq ($(OS),darwin)
define rule/exe
$1$(EXEEXT): $~$1.exe FORCE
	$$(if $$(NOLINK),:,$(FASTCP) $$< $$@)

$(eval $(call fun/foreach-ext-rule,$1,$~$1.exe,$($1_SOURCES),$4))
$~$1.exe:
	$(msg/LINK.c) $$(@R)
	$$(if $$(NOLINK),:,ld -o $$@ $$(filter %.o %.oo %.ld %.a,$$^)    \
	    $$(addprefix -force_load ,$$(filter %.wa,$$^))              \
	    -e main -macosx_version_min 10.8                             \
	    -lc $$(if $$(filter clang++,$$(_L)),-lstdc++)                \
	    $(filter-out -lrt,$(LIBS) $($(1DV)_LIBS) $($(1D)_LIBS) $($1_LIBS)))
$(1DV)clean::
	$(RM) $1$(EXEEXT)
endef
else
define rule/exe
$1$(EXEEXT): $~$1.exe FORCE
	$$(if $$(NOLINK),:,$(FASTCP) $$< $$@)

$(eval $(call fun/foreach-ext-rule,$1,$~$1.exe,$($1_SOURCES),$4))
$~$1.exe: _L=$(or $($1_LINKER),$(CC))
$~$1.exe:
	$(msg/LINK.c) $$(@R)
	$$(if $$(NOLINK),:,$$(_L) $(CFLAGS) $($(1DV)_CFLAGS) $($1_CFLAGS) \
	    -o $$@ $$(filter %.o %.oo %.ld,$$^) \
	    $$(LDFLAGS) $$(LDNOPICFLAGS) $$($(1DV)_LDFLAGS) $$($(1D)_LDFLAGS) $$($1_LDFLAGS) \
	    -Wl,--whole-archive $$(filter %.wa,$$^) \
	    -Wl,--no-whole-archive $$(filter %.a,$$^) \
		$$(if $$(filter clang++,$$(_L)),-lstdc++) \
	    $(LIBS) $($(1DV)_LIBS) $($(1D)_LIBS) $($1_LIBS))
$(1DV)clean::
	$(RM) $1$(EXEEXT)
endef
endif

define rule/program
$(1DV)all:: $1$(EXEEXT)
$(eval $(call rule/exe,$1,$2,$3))
endef

#}}}
#[ _CSS ]#############################################################{{{#

define rule/css
$(1DV)www:: $(1DV)$1
$(eval $(call fun/foreach-ext-rule-nogen,$1,$(1DV)$1,$($1_SOURCES)))
endef

#}}}
#[ _JS ]##############################################################{{{#

define rule/js
$(1DV)www:: $~$1/.mark $(1DV)$1
$~$1/.build: $(foreach e,$($1_SOURCES),$e $(wildcard $e/**/*.js) $(wildcard $e/**/*.json))
$~$1/.build: | _generated_hdr
	mkdir -p $$(dir $$@)
	rsync --delete -r -k -K -H --exclude=".git" $($1_SOURCES) $$(dir $$@)
	touch $~$1/.build

$~$1/.mark: $~$1/.build $($1_CONFIG)
	$(msg/COMPILE.js) $($1_CONFIG)
	r.js -o $($1_CONFIG) baseUrl=$~$1/javascript > $~rjs.log \
		|| (cat $~rjs.log; false)
	touch $~$1/.mark

$($1_MINIFY): $~$1/.mark
$(eval $(call fun/foreach-ext-rule-nogen,$1,$(1DV)$1,$($1_MINIFY)))
endef

$(eval $(call fun/common-depends,$1,$~$1/.build,$1))
#}}}
#[ _DATAS ]###########################################################{{{#

define rule/datas
$(1DV)all:: $1
$(eval $(call fun/foreach-ext-rule,$1,$1,$($1_SOURCES)))
endef

#}}}
#[ _DOCS ]############################################################{{{#

define ext/rule/xml
$~$1: $(var/docdir)/dblatex/intersec.specs
$~$1: $(var/docdir)/dblatex/highlight.pl $(var/docdir)/dblatex/asciidoc-dblatex.xsl
$~$1: $3
	$(msg/DOC.pdf) $1
	xmllint --valid $< >/dev/null
	dblatex -q -r $(var/docdir)/dblatex/highlight.pl \
		-p $(var/docdir)/dblatex/asciidoc-dblatex.xsl \
		--param=doc.lot.show=figure,table \
		$(DBLATEXFLAGS) $($(1DV)/_DBLATEXFLAGS) $($1_DBLATEXFLAGS) \
		-I $(1D) -T $(var/docdir)/dblatex/intersec.specs $3 -o $$@+
	$(MV) $$@+ $$@ && chmod a-w $$@
endef

define ext/expand/adoc
ifeq ($(filter %.inc.adoc,$3),)
$~$1.xml: FL_=$($(1DV)_ASCIIDOCFLAGS) $($1_ASCIIDOCFLAGS)
$~$1.xml: $3 $(3:%.adoc=%-docinfo.xml)
	$(msg/DOC.adoc) $3
	asciidoc -b docbook -a docinfo -a toc -a ascii-ids $$(FL_) \
		-f $(var/cfgdir)/asciidoc.conf -o $$@+ $$<
	$(MV) $$@+ $$@ && chmod a-w $$@
else
$~$1.xml: $3
endif
endef

define ext/rule/adoc
$$(foreach t,$3,$$(eval $$(call fun/do-once,$$t,$$(call ext/expand/adoc,$1,$2,$$t,$4))))
$(eval $(call fun/common-depends,$1,$(3:%=$~%.xml),$3))
$(eval $(call ext/rule/xml,$1,$2,$(1:%=$~%.xml),$4))
endef

define ext/expand/sdf
$~$3.inc.adoc: $3 $~$(patsubst $(var/srcdir)%,%,$(var/platform))/qrrd/sdf2adoc.exe
	mkdir -p $$(@D)
	$(msg/DOC.sdf) $3
	$~$(patsubst $(var/srcdir)%,%,$(var/platform))/qrrd/sdf2adoc.exe -o $$@+ $$<
	$(MV) $$@+ $$@ && chmod a-w $$@
	$(FASTCP) $$@ $3.inc.adoc
$(call ext/expand/adoc,$1,$2,$~$3.inc.adoc,$4)
endef

define ext/rule/sdf
$$(foreach t,$3,$$(eval $$(call fun/do-once,$$t,$$(call ext/expand/sdf,$1,$2,$$t,$4))))
endef

define ext/expand/txt
ifeq ($(%-MIB.txt,$3),)
$~$3.inc.adoc: FL_=$($(1DV)_MIBFLAGS) $($1_MIBFLAGS) $($3_MIBFLAGS)
$~$3.inc.adoc: $3
	mkdir -p $$(@D)
	$(msg/DOC.mib) $3
	smidump -q -f python -k $$(FL_) -o $$@.py $$<
	cat $(var/cfgdir)/mib.py >> $$@.py

	python $$@.py > $$@+
	$(MV) $$@+ $$@ && chmod a-w $$@
	$(FASTCP) $$@ $3.inc.adoc
$(call ext/expand/adoc,$1,$2,$~$3.inc.adoc,$4)
endif
endef

define ext/rule/txt
$(eval $(call fun/common-depends,$1,$(3:%=$~%.inc.adoc),$3))
$$(foreach t,$3,$$(eval $$(call fun/do-once,$$t,$$(call ext/expand/txt,$1,$2,$$t,$4))))
endef

define rule/pdf
$1: $~$1 FORCE
	$(FASTCP) $$< $$@

$(eval $(call fun/foreach-ext-rule-nogen,$1,$~$1,$(if $($1_SOURCES),$($1_SOURCES),$(1:%.pdf=%.adoc)),$4))

$(1DV)clean::
	$(RM) $1
endef

define rule/docs
$(1DV)doc: $1
$(eval $(call rule/pdf,$1,$2,$3))
endef

#}}}
