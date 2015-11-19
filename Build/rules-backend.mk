##########################################################################
#                                                                        #
#  Copyright (C) 2004-2015 INTERSEC SA                                   #
#                                                                        #
#  Should you receive a copy of this source code, you must check you     #
#  have a proper, written authorization of INTERSEC to hold it. If you   #
#  don't have such an authorization, you must DELETE all source code     #
#  files in your possession, and inform INTERSEC of the fact you obtain  #
#  these files. Should you not comply to these terms, you can be         #
#  prosecuted in the extent permitted by applicable law.                 #
#                                                                        #
##########################################################################

ifeq (,$(NOCOMPRESS))

define fun/obj-compress
objcopy --compress-debug-sections $1 >/dev/null 2>&1 || true
endef

ifeq (,$(filter %compress-debug%,$(LDFLAGS)))
define fun/bin-compress
objcopy --compress-debug-sections $1 >/dev/null 2>&1 || true
endef
else
define fun/bin-compress
endef
endif

else # NOCOMPRESS
define fun/obj-compress
endef

define fun/bin-compress
endef
endif

#
# extension driven rules
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
	$(call fun/obj-compress,$5)
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
	$(call fun/obj-compress,$5)
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
	       -e 's/^\t\tint n; \\/            size_t n; \\/' \
	       $(if $(flex_2537),-e 's/^\tint i;/    yy_size_t i;/',) \
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
#[ java ]#############################################################{{{#

# ext/expand/java <PHONY>,<TARGET>,<JAVA>,<CLASS>
#
# To avoid race conditions where several javac commands try to write in
# the same file, each is directed in its own directory (javatmp/classname),
# before copying the relevant class files back in the build directory.
define ext/expand/java
$4: CLASSNAME_=$(basename $(notdir $3))
$4: $3
	mkdir -p $$(@D)/javatmp/$$(CLASSNAME_)
	$(msg/COMPILE.java) $3
	javac -classpath "$$($1_CLASSPATH):$(1DV)" -d $$(@D)/javatmp/$$(CLASSNAME_) $$<
	cp $$(@D)/javatmp/$$(CLASSNAME_)/*.class $$(@D)

$2: $4
endef

define ext/rule/java
$$(foreach t,$3,$$(eval $$(call fun/do-once,$$t,\
	$$(call ext/expand/java,$1,$2,$$t,$~$$(t:java=class)))))
$(eval $(call fun/common-depends,$1,$(3:java=class),$3))
endef

#}}}

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
	$$(if $$(NOLINK),:,$$(call fun/bin-compress,$$@))

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
	$$(if $$(NOLINK),:,$$(call fun/bin-compress,$$@))

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
	$$(if $$(NOLINK),:,$$(call fun/bin-compress,$$@))
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
	$$(if $$(NOLINK),:,$$(call fun/bin-compress,$$@))
$(1DV)clean::
	$(RM) $1$(EXEEXT)
endef
endif

define rule/program
$(1DV)all:: $1$(EXEEXT)
$(eval $(call rule/exe,$1,$2,$3))
endef

#}}}
#[ _JARS ]###########################################################{{{#

define rule/jars
$(1DV)all:: $1.jar
$(eval $(call fun/foreach-ext-rule,$1,$1.jar,$($1_SOURCES)))
$1.jar:
	$(msg/LINK.jar) $$(@R)

    # * Go where the class files are to build the jar file, to avoid jaring the build directory
	# * Do not add %.class but %*.class, as a compiled java file may generate several class files
	cd $~$(1DV) && jar cf $$(patsubst $(1DV)%,%,$$@) $$(patsubst $~$(1DV)%.class,%*.class,$$(filter %.class,$$^))

	cp -f $~$1.jar $$@

$(1DV)clean::
	$(RM) $1.jar
endef

#}}}
