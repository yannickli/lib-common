##########################################################################
#                                                                        #
#  Copyright (C) 2004-2009 INTERSEC SAS                                  #
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
var/srcdir    := $(realpath $(dir $(var/toolsdir)))
var/cfgdir    ?= $(realpath $(var/srcdir)/Config)
var/profile   := $(or $(P),$(PROFILE),$(BUILDFOR),default)
var/builddir  := $(var/srcdir)/.build-$(var/profile)-$(shell hostname)
/             := $(var/srcdir)/
!             := $(var/builddir)/
~             := .build-$(var/profile)-$(shell hostname)/

var/verbose   := $(V)$(VERBOSE)
var/nocolor   := $(M)$(NOCOLOR)$(MONOCHROME)
var/squote    := $(shell echo "'")
var/tab       := $(shell printf '\t')
var/uname     := $(shell uname -s)

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
msg/generate  := $(msg/color) '0;33' " GEN"
msg/depends   := $(msg/color) '0;33' " DEP"
msg/COMPILE   := $(msg/color) '0;33'
msg/COMPILE.l := $(msg/color) '0;33' " LEX"
msg/COMPILE.c := $(msg/color) '1;34' " CC "
msg/LINK.a    := $(msg/color) '1;35' " AR "
msg/LINK.c    := $(msg/color) '1;36' " LD "
msg/rm        := $(msg/echo)  " RM "

##########################################################################
# Setup make properly
#
.SUFFIXES:
.DEFAULT_GOAL := all
SUFFIXES      :=
MAKEFLAGS     := $(MAKEFLAGS)r$(if $(var/verbose),,s)
ifeq (,$(L)$(LINEAR))
MAKEPARALLEL  := $(MAKE) -j$(shell expr $$(getconf _NPROCESSORS_ONLN) + 1)
else
MAKEPARALLEL  := $(MAKE)
endif

##########################################################################
# our own automatic variables
#
@R = $(patsubst $~%,%,$(patsubst $/%,%,$@))
<R = $(patsubst $~%,%,$(patsubst $/%,%,$<))
^R = $(patsubst $~%,%,$(patsubst $/%,%,$^))

1D = $(patsubst %/,%,$(dir $1))
2D = $(patsubst %/,%,$(dir $1))

1F = $(notdir $1)
2F = $(notdir $1)

##########################################################################
# Custom tools
#
FASTCP := ln -f

##########################################################################
# Load our build profile
#
include $(var/cfgdir)/profile-$(var/profile).mk
