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

ifndef .FEATURES
    $(error This build system requires make 3.81 or later)
endif

var/toolsdir  := $(realpath $(dir $(lastword $(MAKEFILE_LIST))))

# Quite hacky way to detect if we are a subdirectory or not.
var/libcommon := $(realpath $(var/toolsdir)/..)
var/srcdir    := $(realpath $(shell $(var/toolsdir)/_find_root.sh "$(var/libcommon)"))
ifneq (,$(filter %/lib-common,$(subst T$(var/srcdir),,T$(var/libcommon))))
	var/platform := $(realpath $(var/libcommon)/..)
else
	var/platform := $(var/libcommon)
endif

var/cfgdir    ?= $(realpath $(var/toolsdir)/../Config)
var/docdir    ?= $(realpath $(var/toolsdir)/../Documentation)
var/profile   := $(or $(P),$(PROFILE),$(BUILDFOR),default)
var/hostname  ?= $(shell hostname)
var/builddir  ?= $(var/srcdir)/.build-$(var/profile)-$(var/hostname)
/             := $(var/srcdir)/
!             := $(var/builddir)/
~             := .build-$(var/profile)-$(var/hostname)/
l             := $(var/libcommon)/
p             := $(var/platform)
var/wwwtool   := $/node_modules/.bin/
export var/hostname

var/verbose   := $(V)$(VERBOSE)
var/nocolor   := $(M)$(NOCOLOR)$(MONOCHROME)
var/squote    := $(shell echo "'")
var/tab       := $(shell printf '\t')
var/uname     := $(shell uname -s)
var/comma     := ,

ifeq (,$(var/verbose))
    msg/echo  := echo
ifeq (,$(var/nocolor))
    msg/color := /usr/bin/printf '\033[%sm%s %s\033[0m\n'
else
    msg/color := /usr/bin/printf '%.0s%s %s\n'
endif
else
    msg/echo  := @:
    msg/color := @:
endif

col/black       := 30
col/red         := 31
col/green       := 32
col/yellow      := 33
col/blue        := 34
col/magenta     := 35
col/cyan        := 36
col/white       := 37
col/default     := 38

col/bg_black    := 40
col/bg_red      := 41
col/bg_green    := 42
col/bg_yellow   := 43
col/bg_blue     := 44
col/bg_magenta  := 45
col/bg_cyan     := 46
col/bg_white    := 47
col/bg_default  := 48

msg/generate    := $(msg/color) '0;$(col/yellow)'  " GEN"
msg/depends     := $(msg/color) '0;$(col/yellow)'  " DEP"
msg/npm         := $(msg/color) '0;$(col/yellow)'  " NPM"
msg/CHECK.c     := $(msg/color) '0;$(col/green)'   ".CC "
msg/CHECK.C     := $(msg/color) '0;$(col/green)'   ".CXX"
msg/CHECK.py    := $(msg/color) '0;$(col/green)'   ".PY "
msg/CHECK.js    := $(msg/color) '0;$(col/green)'   ".JS "
msg/CHECK.ts    := $(msg/color) '0;$(col/green)'   ".TS "
msg/COMPILE     := $(msg/color) '1;$(col/blue)'
msg/COMPILE.c   := $(msg/color) '1;$(col/blue)'    " CC "
msg/COMPILE.C   := $(msg/color) '1;$(col/blue)'    " CXX"
msg/COMPILE.l   := $(msg/color) '0;$(col/yellow)'  " LEX"
msg/COMPILE.iop := $(msg/color) '0;$(col/cyan)'    " IOP"
msg/COMPILE.less := $(msg/color) '0;$(col/blue)'   " LSS"
msg/COMPILE.js  := $(msg/color) '0;$(col/blue)'    " RJS"
msg/COMPILE.ts  := $(msg/color) '0;$(col/blue)'    " TSC"
msg/COMPILE.json:= $(msg/color) '0;$(col/blue)'    " CP "
msg/COMPILE.java := $(msg/color) '0;$(col/blue)'   " JVA"
msg/MINIFY.css  := $(msg/color) '0;$(col/cyan)'    " MIN"
msg/MINIFY.js   := $(msg/color) '0;$(col/cyan)'    " MIN"
msg/LINK.jar    := $(msg/color) '1;$(col/green)'   " JAR"
msg/LINK.a      := $(msg/color) '1;$(col/magenta)' " AR "
msg/LINK.c      := $(msg/color) '1;$(col/cyan)'    " LD "
msg/LINK.js     := $(msg/color) '1;$(col/cyan)'    " BRW"
msg/rm          := $(msg/color) '0;$(col/default)' " RM "
msg/alert       := $(msg/color) '1;$(col/white)$(col/bg_red)' "***"
msg/DOC.adoc    := $(msg/color) '0;$(col/red)'     " DOC"
msg/DOC.mib     := $(msg/color) '0;$(col/red)'     " MIB"
msg/DOC.sdf     := $(msg/color) '1;$(col/red)'     " SDF"
msg/DOC.pdf     := $(msg/color) '1;$(col/red)'     " PDF"

##########################################################################
# Setup make properly
#
.SUFFIXES:
.DEFAULT_GOAL := all
SUFFIXES      :=
MAKEFLAGS     := $(MAKEFLAGS)r$(if $(var/verbose),,s)
PARALLELISM   := $(shell $(var/toolsdir)/getncpu.sh)
SUBPARALLELISM := $(shell echo $$(( ($(PARALLELISM) + 1) / 2 )))
ifeq (,$(L)$(LINEAR))
MAKEPARALLEL  ?= $(MAKE) -j$(PARALLELISM)
else
MAKEPARALLEL  ?= $(MAKE)
endif

##########################################################################
# our own automatic variables
#
@R = $(patsubst $~%,%,$(patsubst $/%,%,$@))
<R = $(patsubst $~%,%,$(patsubst $/%,%,$<))
^R = $(patsubst $~%,%,$(patsubst $/%,%,$^))

1D = $(patsubst %/,%,$(dir $1))
2D = $(patsubst %/,%,$(dir $1))

# 1V is the prefix to use for the directory variables
1DV = $(if $(patsubst .%,%,$(1D)),$(1D)/,)
2DV = $(if $(patsubst .%,%,$(2D)),$(2D)/,)

1F = $(notdir $1)
2F = $(notdir $1)

##########################################################################
# Custom tools
#
FASTCP := ln -f
MV     := mv -f
INSTALL := $(shell which ginstall 2> /dev/null)
ifeq ($(INSTALL),)
	INSTALL := install
endif


##########################################################################
# Load our build profile
#
include $(var/cfgdir)/profile-$(var/profile).mk
