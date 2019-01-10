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
# tools
#
# {{{ tools

$(var/wwwtool)lessc: _npm_tools
$(var/wwwtool)webpack: _npm_tools

$/node_modules/build.lock: $/package.json
	$(msg/npm) ""
	cd $(dir $<) && npm install --quiet
	touch $@

# }}}


#
# extension driven rules
#
# {{{ css

ext/gen/css = $(if $(filter %.css,$1),$(strip $($2_DESTDIR))/$(notdir $(1:css=min.css)))

define ext/expand/css
$(strip $($1_DESTDIR))/$(notdir $(3:css=min.css)): $3 $(var/wwwtool)lessc
	$(msg/MINIFY.css) $3
	mkdir -p `dirname "$~$$@"`
	$(var/wwwtool)lessc -M $$< $$@ > $~$$@.d
	(cat $(var/cfgdir)/head.css && $(var/wwwtool)lessc --clean-css="--s1 --advanced --compatibility=ie8" $$<) > $$@+
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
$(strip $($1_DESTDIR))/$(notdir $(3:less=css)): $3 $(var/wwwtool)lessc
	$(msg/COMPILE.less) $3
	mkdir -p `dirname "$~$$@"`
	$(var/wwwtool)lessc -M $$< $$@ > $~$$@.d
	$(var/wwwtool)lessc --source-map=$$@.map+ $$< $$@+
	$(MV) $$@+ $$@ && chmod a-w $$@
	$(MV) $$@.map+ $$@.map && chmod a-w $$@.map
-include $~$(strip $($1_DESTDIR))/$(notdir $(3:less=css)).d
$2: $(strip $($1_DESTDIR))/$(notdir $(3:less=css))
endef

define ext/rule/less
$$(foreach t,$3,$$(eval $$(call fun/do-once,$$t,$$(call ext/expand/less,$1,$2,$$t,$4))))
$(eval $(call ext/rule/css,$1,$2,$(strip $($1_DESTDIR))/$(notdir $(3:less=css)),$4))
$(eval $(call fun/common-depends,$1,$(strip $($1_DESTDIR))/$(notdir $(3:less=css)),$3))
endef

# }}}

#
# main targets (entry points)
#
#[ _CSS ]#############################################################{{{#

define rule/css
$(1DV)www:: $(1DV)$1
$(eval $(call fun/foreach-ext-rule-nogen,$1,$(1DV)$1,$($1_SOURCES)))
endef

#}}}
#[ _WWWMODULES ]#######################################################{{{#

# rule/webpack <MODULE>,<for www-check?>
#
# Add webpack target in current directory
define rule/webpack
ifeq (,$2)
$(1DV)www:: $1
else
$(1DV)www-check-deps:: $1
endif
$1: _CFGFILE=$(or $(patsubst $(1DV)%,%,$($1_CONFIG)),webpack.config.js)
$1: _MODE=$(if $(WEBPACK_MODE),--mode $(WEBPACK_MODE))
$1: _CHECK=$(if $(NOCHECK),--nocheck)
$1: _DEV=$(if $(WWW_DEV),--mode development --watch)
$1: $(var/wwwtool)webpack
$1: FORCE
	$(msg/PACK.js) $1
	cd $(1DV) && node --max-old-space-size=4096 $(var/wwwtool)webpack --config $$(_CFGFILE) $$(_MODE) $$(_CHECK) $$(_DEV)
endef

#}}}
