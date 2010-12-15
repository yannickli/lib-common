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

__$(1D)_generated: $$(tmp/$2/toks_h) $$(tmp/$2/toks_c)
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

__$(1D)_generated: $(3:.perf=.c)
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

__$(1D)_generated: $(3:.lua=.lc.bin)
$$(eval $$(call fun/common-depends,$1,$(3:.lua=.lc.bin),$3))
.INTERMEDIATE: $(3:lua=lc)
endef

define ext/rule/lua
$$(foreach t,$3,$$(eval $$(call fun/do-once,$$t,$$(call ext/expand/lua,$1,$2,$$t,$4))))
endef

#}}}
#[ farchs/fcs ]#######################################################{{{#

ext/gen/farch = $(call fun/patsubst-filt,%.farch,%farch.h,$1) $(call fun/patsubst-filt,%.farch,%farch.c,$1)

define ext/rule/farch
$(3:.farch=farch.c): %farch.c: %farch.h
$(3:.farch=farch.h): %farch.h: %.farch
	$(msg/generate) $$(<R)
	cd $$(@D) && buildfarch -r $(1D)/ -d $!$$@.dep -n $$(*F) `cat $$(<F)`

$$(eval $$(call ext/rule/c,$1,$2,$(3:.farch=farch.c),$4))
__$(1D)_generated: $(3:.farch=farch.h) $(3:.farch=farch.c)
-include $$(patsubst %,$~%.c.dep,$3)
$$(eval $$(call fun/common-depends,$1,$(3:.farch=farch.h) $(3:.farch=farch.c),$3))
endef

# -- fcs

ext/gen/fc = $(call fun/patsubst-filt,%.fc,%.fc.c,$1)

define ext/rule/fc
$(3:=.c): %.fc.c: %.fc
	$(msg/generate) $$(<R)
	farchc -d $~$$@.dep -o $$@ $$<
__$(1D)_generated: $(3:=.c)
-include $$(patsubst %,$~%.c.dep,$3)
$$(eval $$(call fun/common-depends,$1,$(3:=.c),$3))
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

ext/gen/blk = $(call fun/patsubst-filt,%.blk,%.c,$1)

define ext/expand/blk
$(3:blk=c): %.c: %.blk $(shell which clang)
	$(msg/COMPILE) " BLK" $$(<R)
	$(RM) $$@
	clang -cc1 $$(CLANGFLAGS) \
	    $$($(1D)/_CFLAGS) $$($1_CFLAGS) $$($$*.c_CFLAGS) \
	    -fblocks -rewrite-blocks -o $$@ $$<
	chmod a-w $$@
__$(1D)_generated: $(3:blk=c)
$$(eval $$(call fun/common-depends,$1,$(3:blk=c),$3))
endef

define ext/rule/blk
$$(foreach t,$3,$$(eval $$(call fun/do-once,$$t,$$(call ext/expand/blk,$1,$2,$$t,$4))))
$$(eval $$(call ext/rule/c,$1,$2,$(3:blk=c),$4))
endef

#}}}
