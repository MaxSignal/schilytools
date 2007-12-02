#ident @(#)suntar.mk	1.15 07/02/17 
###########################################################################
#include		$(MAKE_M_ARCH).def
SRCROOT=	..
RULESDIR=	RULES
include		$(SRCROOT)/$(RULESDIR)/rules.top
###########################################################################

INSDIR=		bin
TARGET=		suntar
#SYMLINKS=	ustar tar
CPPOPTS +=	-D__STAR__
CPPOPTS +=	-DSET_CTIME -DFIFO -DUSE_MMAP -DUSE_REMOTE -DUSE_RCMD_RSH
#CPPOPTS +=	-DSET_CTIME -DFIFO -DUSE_MMAP
#CPPOPTS +=	-DSET_CTIME -DUSE_MMAP
#CPPOPTS +=	-DFIFO -DUSE_MMAP
CPPOPTS +=	-DUSE_LARGEFILES
CPPOPTS +=	-DUSE_ACL
CPPOPTS +=	-DUSE_XATTR
CPPOPTS +=	-DUSE_FFLAGS
CPPOPTS +=	-DSCHILY_PRINT
CFILES=		suntar.c header.c cpiohdr.c xheader.c xattr.c \
		list.c extract.c create.c append.c diff.c restore.c \
		remove.c star_unix.c acl_unix.c acltext.c fflags.c \
		buffer.c dirtime.c lhash.c \
		hole.c longnames.c names.c \
		movearch.c table.c props.c \
		fetchdir.c \
		unicode.c \
		subst.c volhdr.c \
		chdir.c match.c defaults.c dumpdate.c \
		fifo.c device.c checkerr.c
HFILES=		star.h starsubs.h dirtime.h xtab.h xutimes.h \
		movearch.h table.h props.h fifo.h diff.h \
		checkerr.h dumpdate.h bitstring.h
#LIBS=		-lunos
#LIBS=		-lschily -lc /usr/local/lib/gcc-gnulib
LIBS=		-ldeflt -lrmt -lschily $(LIB_ACL) $(LIB_ATTR) $(LIB_SOCKET)
XMK_FILE=	suntarman.mk

###########################################################################
include		$(SRCROOT)/$(RULESDIR)/rules.cmd
###########################################################################
count:	$(CFILES) $(HFILES)
	count $r1

