#[ tokens ]###########################################################{{{#

ext/gen/tokens = $(call fun/patsubst-filt,%.tokens,%tokens.h,$1) $(call fun/patsubst-filt,%.tokens,%tokens.c,$1)

define ext/expand/tokens
tmp/$2/toks_h := $(patsubst %.tokens,%tokens.h,$3)
tmp/$2/toks_c := $(patsubst %.tokens,%tokens.c,$3)

$$(tmp/$2/toks_h): %tokens.h: %.tokens $(var/cfgdir)/_tokens.sh
	$(msg/generate) $$(<R)
	cd $$(<D) && $(var/cfgdir)/_tokens.sh $$(<F) $$(@F) || ($(RM) $$(@F) && exit 1)

$$(tmp/$2/toks_c): %tokens.c: %.tokens %tokens.h $(var/cfgdir)/_tokens.sh
	$(msg/generate) $$(<R)
	cd $$(<D) && $(var/cfgdir)/_tokens.sh $$(<F) $$(@F) || ($(RM) $$(@F) && exit 1)

_generated_hdr: $$(tmp/$2/toks_h)
_generated: $$(tmp/$2/toks_c)
$$(eval $$(call fun/common-depends,$1,$$(tmp/$2/toks_h) $$(tmp/$2/toks_c),$3))
endef

define ext/rule/tokens
$$(foreach t,$3,$$(eval $$(call fun/do-once,$$t-tok,$$(call ext/expand/tokens,$1,$2,$$t,$4))))
$$(eval $$(call ext/rule/c,$1,$2,$(3:.tokens=tokens.c),$4))
endef

#}}}
#[ perf ]#############################################################{{{#

ext/gen/perf = $(call fun/patsubst-filt,%.perf,%.c,$1)

define ext/expand/perf

$(3:.perf=.c): %.c: %.perf
	$(msg/generate) $$(<R)
	gperf --language=ANSI-C --output-file=$$@ $$<

_generated: $(3:.perf=.c)
$$(eval $$(call fun/common-depends,$1,$(3:.perf=.c),$3))
endef

define ext/rule/perf
$$(foreach t,$3,$$(eval $$(call fun/do-once,$$t-tok,$$(call ext/expand/perf,$1,$2,$$t,$4))))
$$(eval $$(call ext/rule/c,$1,$2,$(3:.perf=.c),$4))
endef

#}}}
#[ lua ]##############################################################{{{#

ext/gen/lua = $(call fun/patsubst-filt,%.lua,%.lc.bin,$1)

define ext/expand/lua
$(3:lua=lc): %.lc: %.lua
	$(msg/COMPILE) " LUA" $$(<R)
	luac -o $$@ $$<

$(3:lua=lc.bin): %.lc.bin: %.lc
	blob2c $$< > $$@ || ($(RM) $$@; exit 1)

_generated: $(3:.lua=.lc.bin)
$$(eval $$(call fun/common-depends,$1,$(3:.lua=.lc.bin),$3))
.INTERMEDIATE: $(3:lua=lc)
endef

define ext/rule/lua
$$(foreach t,$3,$$(eval $$(call fun/do-once,$$t,$$(call ext/expand/lua,$1,$2,$$t,$4))))
endef

#}}}
#[ fc ]###############################################################{{{#

ext/gen/fc = $(call fun/patsubst-filt,%.fc,%.fc.c,$1)

define ext/expand/fc
$$(patsubst %,$~%.c.dep,$3): $~%.c.dep: %
	$(msg/generate) $$(<R)
	farchc -d $~$3.c.dep -o $3.c $$<
$(3:=.c): %.fc.c: %.fc
	$(msg/generate) $$(<R)
	farchc -d $~$3.c.dep -o $3.c $$<
_generated: $(3:=.c)
-include $$(patsubst %,$~%.c.dep,$3)
$$(eval $$(call fun/common-depends,$1,$(3:=.c),$3))
endef

define ext/rule/fc
$$(foreach t,$3,$$(eval $$(call fun/do-once,$$t,$$(call ext/expand/fc,$1,$2,$$t,$4))))
endef

#}}}
#[ iop ]##############################################################{{{#

ext/gen/iop = $(call fun/patsubst-filt,%.iop,%.iop.c,$1) \
    $(call fun/patsubst-filt,%.iop,%.iop.h,$1)

define ext/expand/iop
$(3:=.h): %.h: %.c
$$(patsubst %,$~%.dep,$3): $~%.dep: % $(IOPC)
	$(msg/COMPILE.iop) $$(<R)
	$(RM) $$@
	$(IOPC) --c-resolve-includes --Wextra -d$~$$<.dep -I$/lib-inet:$/qrrd/iop $$<
$(3:=.c): %.iop.c: %.iop $(IOPC)
	$(msg/COMPILE.iop) $$(<R)
	$(RM) $$@
	$(IOPC) --c-resolve-includes --Wextra -d$~$$<.dep -I$/lib-inet:$/qrrd/iop $$<
_generated_hdr: $(3:=.h)
_generated: $(3:=.c)
$$(eval $$(call fun/common-depends,$1,$(3:=.c),$3))
-include $$(patsubst %,$~%.dep,$3)
endef

define ext/rule/iop
$$(foreach t,$3,$$(eval $$(call fun/do-once,$$t,$$(call ext/expand/iop,$1,$2,$$t,$4))))
$$(eval $$(call ext/rule/c,$1,$2,$(3:=.c),$4))
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
$$(eval $$(call fun/common-depends,$1,$(3:.swfml=.swf),$3))
endef

#}}}
#[ blocks ]###########################################################{{{#

ext/gen/blk = $(call fun/patsubst-filt,%.blk,%.blk.c,$1)

define ext/expand/blk
$$(foreach t,$3,$$(eval $3.c_NOCHECK = block))
$(3:=.c): %.c: % $(CLANG)
	$(msg/COMPILE) " BLK" $$(<R)
	$(RM) $$@
	$(CLANG) -cc1 $$(CLANGFLAGS) \
	    $$($(1D)/_CFLAGS) $$($1_CFLAGS) $$($$@_CFLAGS) \
	    -rewrite-blocks -o $$@ $$<
	chmod a-w $$@
_generated: $(3:=.c)
$$(eval $$(call fun/common-depends,$1,$(3:=.c),$3))
endef

define ext/rule/blk
$$(foreach t,$3,$$(eval $$(call fun/do-once,$$t,$$(call ext/expand/blk,$1,$2,$$t,$4))))
$$(eval $$(call ext/rule/c,$1,$2,$(3:=.c),$4))
endef


ext/gen/blkk = $(call fun/patsubst-filt,%.blkk,%.blkk.cc,$1)

define ext/expand/blkk
$$(foreach t,$3,$$(eval $3.cc_NOCHECK = block))
$(3:=.cc): %.cc: % $(CLANGXX)
	$(msg/COMPILE) " BLK" $$(<R)
	$(RM) $$@
	$(CLANGXX) -cc1 $$(CLANGXXFLAGS) \
	    $$($(1D)/_CXXFLAGS) $$($1_CXXFLAGS) $$($$@_CXXFLAGS) \
	    -rewrite-blocks -o $$@ $$<
	chmod a-w $$@
__$(1D)_generated: $(3:=.cc)
$$(eval $$(call fun/common-depends,$1,$(3:=.cc),$3))
endef

define ext/rule/blkk
$$(foreach t,$3,$$(eval $$(call fun/do-once,$$t,$$(call ext/expand/blkk,$1,$2,$$t,$4))))
$$(eval $$(call ext/rule/cc,$1,$2,$(3:=.cc),$4))
endef

#}}}
