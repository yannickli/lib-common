##########################################################################
#                                                                        #
#  Copyright (C) 2004-2019 INTERSEC SA                                   #
#                                                                        #
#  Should you receive a copy of this source code, you must check you     #
#  have a proper, written authorization of INTERSEC to hold it. If you   #
#  don't have such an authorization, you must DELETE all source code     #
#  files in your possession, and inform INTERSEC of the fact you obtain  #
#  these files. Should you not comply to these terms, you can be         #
#  prosecuted in the extent permitted by applicable law.                 #
#                                                                        #
##########################################################################

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
