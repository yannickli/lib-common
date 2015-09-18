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


#
# extension driven rules
#
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

#
# main targets (entry points)
#
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
	cp -r -L -l -f $($1_SOURCES) $$(dir $$@)
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
