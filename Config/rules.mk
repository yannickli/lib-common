#[ lex ]##############################################################{{{#

ext/gen/l = $(call fun/patsubst-filt,%.l,%.c,$1)

define ext/expand/l
$(3:l=c): %.c: %.l
	$(msg/COMPILE.l) $$(@R)
	flex -R -o $$@ $$<
	sed -i -e 's/^extern int isatty.*;//' \
	       -e 's/^\t\tint n; \\/		size_t n; \\/' $$@
__$(1D)_generated: $(3:l=c)
$$(eval $$(call fun/common-depends,$1,$(3:l=c),$3))
endef

define ext/rule/l
$$(foreach t,$3,$$(eval $$(call fun/do-once,$$t,$$(call ext/expand/l,$1,$2,$$t,$4))))
$$(eval $$(call ext/rule/c,$1,$2,$(3:l=c),$4))
endef

#}}}
#[ tokens ]###########################################################{{{#

ext/gen/tokens = $(call fun/patsubst-filt,%.tokens,%tokens.h,$1) $(call fun/patsubst-filt,%.tokens,%tokens.c,$1)

define ext/expand/tokens
tmp/$2/toks_h := $(patsubst %.tokens,%tokens.h,$3)
tmp/$2/toks_c := $(patsubst %.tokens,%tokens.c,$3)

$$(tmp/$2/toks_h): %tokens.h: %.tokens $(var/cfgdir)/_tokens.sh
	$(msg/generate) $$(@R)
	cd $$(<D) && $(var/cfgdir)/_tokens.sh $$(<F) $$(@F) || ($(RM) $$(@F) && exit 1)

$$(tmp/$2/toks_c): %tokens.c: %.tokens %tokens.h $(var/cfgdir)/_tokens.sh
	$(msg/generate) $$(@R)
	cd $$(<D) && $(var/cfgdir)/_tokens.sh $$(<F) $$(@F) || ($(RM) $$(@F) && exit 1)

__$(1D)_generated: $$(tmp/$2/toks_h) $$(tmp/$2/toks_c)
$$(eval $$(call fun/common-depends,$1,$$(tmp/$2/toks_h) $$(tmp/$2/toks_c),$3))
endef

define ext/rule/tokens
$$(foreach t,$3,$$(eval $$(call fun/do-once,$$t-tok,$$(call ext/expand/tokens,$1,$2,$$t,$4))))
$$(eval $$(call ext/rule/c,$1,$2,$(3:.tokens=tokens.c),$4))
endef

#}}}
#[ lua ]##############################################################{{{#

ext/gen/lua = $(call fun/patsubst-filt,%.lua,%.lc.bin,$1)

define ext/expand/lua
$(3:lua=lc): %.lc: %.lua
	$(msg/COMPILE) " LUA" $$(<R)
	luac -o $$@ $$<

$(3:lua=lc.bin): %.lc.bin: %.lc
	util/bldutils/blob2c $$< > $$@ || ($(RM) $$@; exit 1)

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
$(3:.farch=farch.c): %farch.c: %farch.h util/bldutils/buildfarch
$(3:.farch=farch.h): %farch.h: %.farch  util/bldutils/buildfarch
	$(msg/generate) $$(@R)
	cd $$(@D) && $/util/bldutils/buildfarch -r $(1D)/ -d $!$$@.dep -n $$(*F) `cat $$(<F)`

$$(eval $$(call ext/rule/c,$1,$2,$(3:.farch=farch.c),$4))
__$(1D)_generated: $(3:.farch=farch.h) $(3:.farch=farch.c)
-include $$(patsubst %,$~%.c.dep,$3)
$$(eval $$(call fun/common-depends,$1,$(3:.farch=farch.h) $(3:.farch=farch.c),$3))
endef

# -- fcs

ext/gen/fc = $(call fun/patsubst-filt,%.fc,%.fc.c,$1)

define ext/rule/fc
$(3:=.c): %.fc.c: %.fc util/bldutils/farchc
	$(msg/generate) $$(@R)
	$/util/bldutils/farchc -d $~$$@.dep -o $$@ $$<
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
#[ marshaling ]#######################################################{{{#

ext/gen/bp = $(call fun/patsubst-filt,%.bp,%.bp.h,$1)

define ext/expand/bp
$(3:=.h): %.bp.h: %.bp util/bldutils/bpack
	$(msg/generate) $$(@R)
	util/bldutils/bpack -o $$@ $$<
__$(1D)_generated: $(3:=.h)
$$(eval $$(call fun/common-depends,$1,$(3:=.h),$3))
endef

define ext/rule/bp
$$(foreach t,$3,$$(eval $$(call fun/do-once,$$t,$$(call ext/expand/bp,$1,$2,$$t,$4))))
endef

#}}}
