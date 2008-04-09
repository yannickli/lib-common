##########################################################################
#                                                                        #
#  Copyright (C) 2004-2008 INTERSEC SAS                                  #
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
var/profile   := $(if $(BUILDFOR),$(BUILDFOR),default)
var/builddir  := $(var/srcdir)/.build-$(var/profile)-$(shell hostname)
/             := $(var/srcdir)/
!             := $(var/builddir)/
~             := .build-$(var/profile)-$(shell hostname)/

var/verbose   := $(V)$(VERBOSE)
var/squote    := $(shell echo "'")

ifeq (,$(var/verbose))
    msg/echo := echo
else
    msg/echo := @:
endif
msg/generate  := $(msg/echo) " GEN"
msg/fastcp    := $(msg/echo) " CP "
msg/rm        := $(msg/echo) " RM "
msg/LINK.a    := $(msg/echo) " AR "
msg/COMPILE.c := $(msg/echo) " CC "
msg/COMPILE.l := $(msg/echo) " LEX"
msg/LINK.c    := $(msg/echo) " LD "

##########################################################################
# Setup make properly
#
.SUFFIXES:
.DEFAULT_GOAL := all
SUFFIXES      :=
MAKEFLAGS     := $(MAKEFLAGS)r$(if $(var/verbose),,s)
MAKEPARALLEL  := $(MAKE) -j$(shell echo $$(($$(grep -c processor /proc/cpuinfo) + 1)))

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
include $(var/srcdir)/$(var/profile).mk
