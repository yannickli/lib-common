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
# extension driven rules
#
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
# {{{ js

ext/gen/js = $(call fun/patsubst-filt,%.js,%.min.js,$1)

# ext/expand/legacy/js <PHONY>,<GARBAGE>,<JS>
define ext/expand/legacy/js
$(3:js=min.js): $3
	$(msg/MINIFY.js) $3
	uglifyjs -o $$@+ $$<
	(cat $(var/cfgdir)/head.js && cat $$@+) > $$@
	$(RM) $$@+
$2: $(3:js=min.js)
endef

# ext/expand/js <PHONY>,<TARGET>,<JS>,<MODULEPATH>
#
# Copy the js file in the build directory in order to make it available
# for packaging.
#
# Produces:
# - $~$3: a copy of the source JS file
define ext/expand/js
$2: $~$3
$~$3: $3
	$(msg/COMPILE.json) $3
	mkdir -p $(dir $~$3)
	cp -f $3 $~$3
endef

# Two call patterns:
# - old _JS mode: <PHONY>,<GARBAGE>,<JS>[]
# - new _WWWSCRIPTS: <PHONY>,<TARGET>,<JS>[],<MODULEPATH>
define ext/rule/js
ifeq (,$4)
$$(foreach t,$3,$$(eval $$(call fun/do-once,$$t,$$(call ext/expand/legacy/js,$1,$2,$$t))))
$(eval $(call fun/common-depends,$1,$(3:js=min.js),$3))
else
$$(foreach t,$3,$$(eval $$(call fun/do-once,$$t,$$(call ext/expand/js,$1,$2,$$t,$4,$5))))
endif
endef

# }}}
# {{{ ts

# ext/expand/ts <PHONY>,<TARGET>,<TS>,<MODULEPATH>
#
# Compile the TypeScript file into a javascript object and a declaration
# file. Files must be generated in dependency order since the compiler
# validates the imports.
#
# This also makes sure the resulting declaration does not keeps the
# potential /// <reference> attributes in the files, since they are not
# valid in module declaration files.
#
# Produces:
# - $~$3.d: the dependency file for the typescript module
# - $~$(3:ts=d.ts): the declaration file for the typescript module
# - $~$(3:ts=js): the javascript file for the typescript module
define ext/expand/ts
$2: $~$(3:ts=js)
$~$(3:ts=d.ts): $~$(3:ts=js)
$~$(3:ts=js): $3
	$(msg/COMPILE.ts) $3
	NODE_PATH="$~$4/node_modules:$$(tmp/$1/node_path)" tsc --moduleResolution node --module commonjs --declaration --inlineSourceMap --noImplicitAny --noEmitOnError --removeComments --outDir "$~$(dir $3)" $3
	sed -e 's@///.*<reference.*@@' -i $~$(3:ts=d.ts)

$~$3.d: $3 $(var/toolsdir)/_get_ts_deps.js
	mkdir -p "$$(dir $$@)"
	/bin/echo -n "$~$(3:ts=js): " > $$@+
	NODE_PATH="$4/node_modules:$$(tmp/$1/node_path)" node $(var/toolsdir)/_get_ts_deps.js $$< $/ $~ >> $$@+
	$(MV) $$@+ $$@

-include $~$3.d
endef

# ext/expand/d.ts <PHONY>,<TARGET>,<D.TS>,<MODULEPATH>
#
# Copies the declaration file into the build directory and computes its
# dependences.
#
# Produces:
# - $~$3: a copy of the declaration file
#   $~$3.d: the depency file for the typescript module
define ext/expand/d.ts
$~$3: $3
	$(msg/COMPILE.json) $3
	mkdir -p "$(dir $~$3)"
	cp -f $$< $$@

$~$3.d: $3 $(var/toolsdir)/_get_ts_deps.js
	mkdir -p "$$(dir $$@)"
	/bin/echo -n "$~$3: " > $$@+
	NODE_PATH="$4/node_modules:$$(tmp/$1/node_path)" node $(var/toolsdir)/_get_ts_deps.js $$< $/ $~ >> $$@+
	$(MV) $$@+ $$@

-include $~$3.d
endef

# ext/rule/ts <PHONY>,<TARGET>,<TS>[],<MODULEPATH>
define ext/rule/ts
$$(foreach t,$(filter-out %.d.ts,$3),$$(eval $$(call fun/do-once,$$t,$$(call ext/expand/ts,$1,$2,$$t,$4))))
$$(foreach t,$(filter %.d.ts,$3),$$(eval $$(call fun/do-once,$$t,$$(call ext/expand/d.ts,$1,$2,$$t,$4))))
endef

# }}}
# {{{ json

# ext/expand/json <PHONY>,<TARGET>,<JSON>,<MODULEPATH>
#
# Wraps the JSON file into a javascript module allowing packaging. This
# simply adds a export = { json }. This also produces a module declaration
# file for inclusion of the module in typescript.
#
# Produces:
# - $~$3.js: the JSON wrapped in JavaScript
# - $~$3.d.ts: the declaration file for use with TypeScript
define ext/expand/json
$2: $~$3.js
$~$3.js: $3
	$(msg/COMPILE.json) $3
	mkdir -p "$(dir $~$3)"
	/bin/echo -n "module.exports = " > $$@+
	cat $$< >> $$@+
	$(MV) $$@+ $$@

$~$3.d.ts: $3
	mkdir -p "$(dir $~$3)"
	echo "declare var json: any; export = json;" > $$@+
	$(MV) $$@+ $$@
endef

# ext/rule/json <PHONY>,<TARGET>,<JSON>[],<MODULEPATH>
define ext/rule/json
$$(foreach t,$3,$$(eval $$(call fun/do-once,$$t,$$(call ext/expand/json,$1,$2,$$t,$4))))
endef

# }}}
# {{{ html

# ext/expand/html <PHONY>,<TARGET>,<HTML>,<MODULEPATH>
#
# Wraps the HTML file into a javascript module allowing packaging. This
# simply put the content of the HTML file as a string in the module. This
# also produces a module declaration file for inclusion of the modue in
# typescript code.
#
# Produces:
# - $~$3.js: the HTML wrapped in JavaScript
# - $~$3.d.ts: the declaration file for use with typescript
define ext/expand/html
$2: $~$3.js
$~$3.js: $3
	$(msg/COMPILE.json) $3
	mkdir -p "$(dir $~$3)"
	echo 'module.exports = "" +' > $$@+
	cat $$< | sed -e :a -re 's/<!--.*?-->//g;/<!--/N;//ba' | sed -e 's/\\/\\\\/g' -e 's/"/\\"/g' -e 's/^\(.*\)$$$$/"\1\\n" +/' >> $$@+
	echo '"";' >> $$@+
	$(MV) $$@+ $$@

$~$3.d.ts: $3
	mkdir -p "$(dir $~$3)"
	echo "declare var html: string; export = html;" > $$@+
	$(MV) $$@+ $$@
endef

# ext/rule/html <PHONY>,<TARGET>,<HTML>[],<MODULEPATH>
define ext/rule/html
$$(foreach t,$3,$$(eval $$(call fun/do-once,$$t,$$(call ext/expand/html,$1,$2,$$t,$4))))
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
#[ _JS ]##############################################################{{{#

define rule/js
$(1DV)www:: $~$1/.mark $(1DV)$1
$~$1/.build: $(foreach e,$($1_SOURCES),$e $(shell find $e -type f -name '*.js' -or -name '*.json' -or -name '*.html' -not -name '* *'))
$~$1/.build: | _generated_hdr
	mkdir -p $$(dir $$@)
	rsync --delete -r -k -K -H --exclude=".git" $($1_SOURCES) $$(dir $$@)
	touch $~$1/.build

$~$1/.mark: $~$1/.build $($1_CONFIG)
	$(msg/COMPILE.js) $($1_CONFIG)
	r.js -o $($1_CONFIG) baseUrl=$~$1/javascript > $~$1/rjs.log \
		|| (cat $~$1/rjs.log; false)
	touch $~$1/.mark

$($1_MINIFY): $~$1/.mark
$(foreach e,$($1_MINIFY),$(e:js=min.js)): $~$1/.mark
$(eval $(call fun/foreach-ext-rule-nogen,$1,$(1DV)$1,$($1_MINIFY)))
endef

$(eval $(call fun/common-depends,$1,$~$1/.build,$1))
#}}}
#[ _WWWMODULES ]#######################################################{{{#

# rule/wwwscript <PHONY>,<MODULEPATH>,<BUNDLE>,<DEPS>[]
#
# Builds the javascript bundles associated with a specific module. This is
# called for every bundle declared in <MODULE>_WWWSCRIPTS and process the list
# of sources for that bundle and produce a single javascript file with all
# of them. The generated file is uglified and compressed.
#
# This rule reads the <BUNDLE>_SOURCES variable to retrieve the list of source
# files. Files can be TypeScript object, JavaScript object (with commonjs module
# syntax) or JSON files. The _SOURCES content is patched so that paths are read
# relative to the node_modules/ directory of the module.
#
# Produces:
# - <MODULEPATH>/htdocs/javascript/<BUNDLE>.js
define rule/wwwscript
tmp/$1/node_path := $(call fun/join,:,$(foreach t,$4,$~$t/node_modules/:$t/node_modules/)):$(dir $(shell which tsc))/../lib/js

$(eval $(call fun/foreach-ext-rule,$1,$~$2/htdocs/javascript/$3.js,$(foreach t,$($(1DV)$3_SOURCES),$(t:$(1DV)%=$2/node_modules/%)),$2))
$(1DV)www:: $2/htdocs/javascript/$3.js
$~$2/htdocs/javascript/$3.js: $~$2/package.json
$~$2/htdocs/javascript/$3.js:
	$(msg/LINK.js) $3.js
	mkdir -p $~$2/htdocs/javascript
	cd $~$2/node_modules/
	NODE_PATH="$$(tmp/$1/node_path)" browserify -g browserify-shim $$(foreach t,$$(filter %.js,$$^),-r $$t:$$(t:$~$2/node_modules/%.js=%)) --no-bundle-external -o $$@

$2/htdocs/javascript/$3.js: $(foreach t,$4,$(foreach s,$($(t:%/modules/$(notdir $t)=%)/$(notdir $t)_WWWSCRIPTS),$(dir $s)modules/$(notdir $t)/htdocs/javascript/$(notdir $s).js))
$2/htdocs/javascript/$3.js: $~$2/htdocs/javascript/$3.js
	$(msg/MINIFY.js) $3.js
	uglifyjs -c warnings=false -m -o $~$$@+ $$<
	$(FASTCP) $~$$@+ $$@
endef

# rule/wwwmodule <MODULE>
#
# Process all the builds associated with a web module. This expands the
# sub targets for the module:
# - <MODULE>_WWWSCRIPTS
define rule/wwwmodule
$~$(1DV)modules/$(1:$(1DV)%=%)/package.json: $(1DV)modules/$(1:$(1DV)%=%)/node_modules/shim.js
	mkdir -p $~$(1DV)modules/$(1:$(1DV)%=%)/node_modules
	$(FASTCP) $(1DV)modules/$(1:$(1DV)%=%)/node_modules/shim.js $~$(1DV)modules/$(1:$(1DV)%=%)/node_modules/shim.js
	echo '{ "browserify-shim": "./node_modules/shim.js" }' > $$@

$$(foreach bundle,$($1_WWWSCRIPTS),$$(eval $$(call fun/do-once,$$(bundle),$$(call rule/wwwscript,$1,$(1DV)modules/$(1:$(1DV)%=%),$$(bundle:$(1DV)%=%),$($1_DEPENDS)))))
endef

#}}}
