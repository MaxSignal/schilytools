#ident @(#)cp1254.mk	1.2 18/03/15 
###########################################################################
# Sample makefile for installing non-localized auxiliary files
###########################################################################
SRCROOT=	../..
RULESDIR=	RULES
include		$(SRCROOT)/$(RULESDIR)/rules.top
###########################################################################

INSDIR=		share/lib/siconv
TARGET=		cp1254
#XMK_FILE=	Makefile.man

###########################################################################
include		$(SRCROOT)/$(RULESDIR)/rules.aux
###########################################################################
