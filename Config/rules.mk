#[ tokens ]###########################################################{{{#

ext/gen/tokens = $(call fun/patsubst-filt,%.tokens,%tokens.h,$1) $(call fun/patsubst-filt,%.tokens,%tokens.c,$1)

define ext/expand/tokens
$(3:.tokens=tokens.h): $3 $(var/cfgdir)/_tokens.sh
	$(msg/generate) $3
	cd $$(<D) && $(var/cfgdir)/_tokens.sh $$(<F) $$(@F) || ($(RM) $$(@F) && exit 1)

$(3:.tokens=tokens.c): $3 $(var/cfgdir)/_tokens.sh | $(3:.tokens=tokens.h)
	$(msg/generate) $3
	cd $$(<D) && $(var/cfgdir)/_tokens.sh $$(<F) $$(@F) || ($(RM) $$(@F) && exit 1)
endef

define ext/rule/tokens
$$(foreach t,$3,$$(eval $$(call fun/do-once,$$t-tok,$$(call ext/expand/tokens,$1,$2,$$t,$4))))
$(eval $(call ext/rule/c,$1,$2,$(3:.tokens=tokens.c),$4))
$(eval $(call fun/common-depends,$1,$(3:.tokens=tokens.h) $(3:.tokens=tokens.c),$3))
_generated_hdr: $(3:.tokens=tokens.h)
_generated: $(3:.tokens=tokens.c)
endef

#}}}
#[ perf ]#############################################################{{{#

ext/gen/perf = $(call fun/patsubst-filt,%.perf,%.c,$1)

define ext/expand/perf
$(3:.perf=.c): $3
	$(msg/generate) $3
	gperf --language=ANSI-C --output-file=$$@ $$<
endef

define ext/rule/perf
$$(foreach t,$3,$$(eval $$(call fun/do-once,$$t-tok,$$(call ext/expand/perf,$1,$2,$$t,$4))))
$(eval $(call ext/rule/c,$1,$2,$(3:.perf=.c),$4))
$(eval $(call fun/common-depends,$1,$(3:.perf=.c),$3))
_generated: $(3:.perf=.c)
endef

#}}}
#[ fc ]###############################################################{{{#

ext/gen/fc = $(call fun/patsubst-filt,%.fc,%.fc.c,$1)

define ext/expand/fc
$~$3.c.dep: $3
	$(msg/generate) $3
	farchc -c -d $~$3.c.dep -o $3.c $$<
$3.c: $3
	$(msg/generate) $3
	farchc -c -d $~$3.c.dep -o $3.c $$<
-include $~$3.c.dep
endef

define ext/rule/fc
$$(foreach t,$3,$$(eval $$(call fun/do-once,$$t,$$(call ext/expand/fc,$1,$2,$$t,$4))))
$(eval $(call fun/common-depends,$1,$(3:=.c),$3))
_generated: $(3:=.c)
endef

#}}}
#[ iop ]##############################################################{{{#

ext/gen/iop = $(call fun/patsubst-filt,%.iop,%.iop.c,$1) \
    $(call fun/patsubst-filt,%.iop,%.iop.h,$1) \
    $(call fun/patsubst-filt,%.iop,%.iop.json,$1)

define ext/expand/iop
$3.h: $3.c
$3.json: $3.c
$3.swift: $3.c
$~$3.dep: IOPINCPATH_=$l $(var/platform)/lib-inet $(var/platform)/qrrd/iop $($(1DV)_IOPINCPATH) $($1_IOPINCPATH) $($3_IOPINCPATH)
$~$3.dep: IOPJSONPATH_=$(firstword $($(1DV)_IOPJSONPATH) $($1_IOPJSONPATH) $($3_IOPJSONPATH) $(1DV))
$~$3.dep: IOPCLASSRANGE_=$(firstword $($(1DV)_IOPCLASSRANGE) $($1_IOPCLASSRANGE) $($3_IOPCLASSRANGE))
$~$3.dep: IOPSWIFTDEPS_=$(firstword $($(1DV)_SWIFTDEPS) $($1_SWIFTDEPS) $($3_SWIFTDEPS))
$~$3.dep: IOPVER_= $(firstword $($3_IOPVER) $($1_IOPVER) $($(1DV)_IOPVER) $(IOPVER))
$~$3.dep: $3 $(IOPC)
	$(msg/COMPILE.iop) $3
	$(RM) $$@
	$(IOPC) $$(IOPVER_) $(IOPFLAGS) -l c,json$(if $(SWIFTC),$(var/comma)swift) \
	    -d$~$$<.dep -I$$(call fun/join,:,$$(IOPINCPATH_)) --json-output-path $$(IOPJSONPATH_) \
	    $$(if $$(IOPCLASSRANGE_),--class-id-range $$(IOPCLASSRANGE_)) \
	    $(if $(SWIFTC),$$(if $$(IOPSWIFTDEPS_),--swift-import-modules $$(IOPSWIFTDEPS_))) $$<
$3.c: IOPINCPATH_=$l $(var/platform)/lib-inet $(var/platform)/qrrd/iop $($(1DV)_IOPINCPATH) $($1_IOPINCPATH) $($3_IOPINCPATH)
$3.c: IOPJSONPATH_=$(firstword $($(1DV)_IOPJSONPATH) $($1_IOPJSONPATH) $($3_IOPJSONPATH) $(1DV))
$3.c: IOPCLASSRANGE_=$(firstword $($(1DV)_IOPCLASSRANGE) $($1_IOPCLASSRANGE) $($3_IOPCLASSRANGE))
$3.c: IOPSWIFTDEPS_=$(firstword $($(1DV)_SWIFTDEPS) $($1_SWIFTDEPS) $($3_SWIFTDEPS))
$3.c: IOPVER_= $(firstword $($3_IOPVER) $($1_IOPVER) $($(1DV)_IOPVER) $(IOPVER))
$3.c: $3 $(IOPC)
	$(msg/COMPILE.iop) $3
	$(RM) $$@
	$(IOPC) $$(IOPVER_) $(IOPFLAGS) -l c,json$(if $(SWIFTC),$(var/comma)swift) \
	    -d$~$$<.dep -I$$(call fun/join,:,$$(IOPINCPATH_)) --json-output-path $$(IOPJSONPATH_) \
	    $$(if $$(IOPCLASSRANGE_),--class-id-range $$(IOPCLASSRANGE_)) \
	    $(if $(SWIFTC),$$(if $$(IOPSWIFTDEPS_),--swift-import-modules $$(IOPSWIFTDEPS_))) $$<
-include $~$3.dep
endef

define ext/rule/iop
$$(foreach t,$3,$$(eval $$(call fun/do-once,$$t,$$(call ext/expand/iop,$1,$2,$$t,$4))))
$(eval $(call fun/common-depends,$1,$(3:=.c),$3))
$(eval $(call ext/rule/c,$1,$2,$(3:=.c),$4))
_generated_hdr: $(3:=.h)
_generated: $(3:=.c)
endef

#}}}
#[ swfml ]############################################################{{{#

ext/gen/swfml = $(call fun/patsubst-filt,%.swfml,%.swf,$1)

define ext/rule/swfml
$$(patsubst %,$~%.dep,$3): $~%.dep: % $(var/cfgdir)/swfml-deps.xsl
	$(msg/depends) $$(<:.swfml=.swf)
	xsltproc --nonet --novalid \
	    --stringparam reldir $$(<D)/ \
	    --stringparam target $$(<F:.swfml=.swf) \
	    -o $$@ $(var/cfgdir)/swfml-deps.xsl $$<
-include $(3:%=$~%.dep)

$(3:.swfml=.swf): %.swf: %.swfml
	$(msg/COMPILE) " SWF" $$@
	cd $$(<D) && swfmill simple $$(<F) $$(@F)
$(eval $(call fun/common-depends,$1,$(3:.swfml=.swf),$3))
endef

#}}}
#[ blocks ]###########################################################{{{#

ifeq ($(filter %clang,$(CC_BASE)),)

ext/gen/blk = $(call fun/patsubst-filt,%.blk,%.blk.c,$1)

define ext/expand/blk
$(foreach t,$3,$(eval $3.c_NOCHECK = block))
$3.c: FL_=$($(1DV)_CFLAGS) $($1_CFLAGS) $($3.c_CFLAGS)
$3.c: FLAGS_=$($(1DV)_CFLAGS) $($1_CFLAGS) $($3_CFLAGS)
$3.c: CLANGFLAGS_=$($(1DV)_CLANGFLAGS) $($1_CLANGFLAGS) $($3_CLANGFLAGS) $$(CLANGFLAGS)
$3.c: $3 $(CLANG) | _generated_hdr
	$(msg/COMPILE) " BLK" $3
	$(CLANG) $$(CLANGFLAGS_) $$(filter-out -D_FORTIFY_SOURCE=%,$$(FLAGS_)) \
		-x c -O0 -fblocks -fsyntax-only -D_FORTIFY_SOURCE=0 \
		-MP -MMD -MT $3.c -MF $~$3.d -o /dev/null $3
	$(CLANG) -cc1 -x c $(CLANGREWRITEFLAGS) $$(FL_) -rewrite-blocks -o $~$3.c+ $$<
	$(MV) $~$3.c+ $$@ && chmod a-w $$@
-include $~$3.d
endef

define ext/rule/blk
$$(foreach t,$3,$$(eval $$(call fun/do-once,$$t,$$(call ext/expand/blk,$1,$2,$$t,$4))))
$(eval $(call fun/common-depends,$1,$(3:=.c),$3))
$(eval $(call ext/rule/c,$1,$2,$(3:=.c),$4))
endef


ext/gen/blkk = $(call fun/patsubst-filt,%.blkk,%.blkk.cc,$1)

define ext/expand/blkk
$(foreach t,$3,$(eval $3.cc_NOCHECK = block))
$3.cc: FL_=$($(1DV)_CXXLAGS) $($1_CXXLAGS) $($3.c_CXXLAGS)
$3.cc: FLAGS_=$($(1DV)_CXXFLAGS) $($1_CXXFLAGS) $($3_CXXFLAGS)
$3.cc: CLANGXXFLAGS_=$($(1DV)_CLANGXXFLAGS) $($1_CLANGXXFLAGS) $($3_CLANGXXFLAGS) $$(CLANGXXFLAGS)
$3.cc: $3 $(CLANGXX) | _generated_hdr
	$(msg/COMPILE) " BLK" $3
	$(CLANGXX) $$(CLANGXXFLAGS_) $$(filter-out -D_FORTIFY_SOURCE=%,$$(FLAGS_)) \
		-x c++ -O0 -fblocks -fsyntax-only -D_FORTIFY_SOURCE=0 \
		-MP -MMD -MT $3.cc -MF $~$3.d -o /dev/null $3
	$(CLANGXX) -cc1 -x c++ $(CLANGXXREWRITEFLAGS) $$(FL_) -rewrite-blocks -o $~$3.cc+ $$<
	$(MV) $~$3.cc+ $$@ && chmod a-w $$@
-include $~$3.d
endef

define ext/rule/blkk
$$(foreach t,$3,$$(eval $$(call fun/do-once,$$t,$$(call ext/expand/blkk,$1,$2,$$t,$4))))
$(eval $(call fun/common-depends,$1,$(3:=.cc),$3))
$(eval $(call ext/rule/cc,$1,$2,$(3:=.cc),$4))
endef

else

ext/rule/blk  = $(ext/rule/c)
ext/rule/blkk = $(ext/rule/cc)

endif

#}}}
