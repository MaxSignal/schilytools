/* @(#)version.h	1.15 11/08/07 Copyright 2007-2011 J. Schilling */

/*
 * The version for sccs programs
 */
#ifndef	_HDR_VERSION_H
#define	_HDR_VERSION_H

#include <schily/mconfig.h>

#ifndef	VERSION
#define	VERSION	"5.02"
#endif

#ifndef	VDATE
#define	VDATE	"2011/08/07"
#endif

#ifdef	SCHILY_BUILD
#	define	PROVIDER	"schily"
#else
#	ifndef	PROVIDER
#	define	PROVIDER	"generic"
#endif
#endif

#ifdef	__sparc
#ifndef	HOST_CPU
#define	HOST_CPU	"sparc"
#endif
#ifndef	HOST_VENDOR
#define	HOST_VENDOR	"Sun"
#endif
#endif /* __sparc */
#if defined(__i386) || defined(__amd64)
#ifndef	HOST_CPU
#define	HOST_CPU	"i386"
#endif
#ifndef	HOST_VENDOR
#define	HOST_VENDOR	"pc"
#endif
#endif	/* defined(__i386) || defined(__amd64) */
#ifndef	HOST_CPU
#define	HOST_CPU	"unknown"
#endif
#ifndef	HOST_VENDOR
#define	HOST_VENDOR	"unknown"
#endif
#ifndef	HOST_OS
#define	HOST_OS		"unknown"
#endif

#endif	/* _HDR_VERSION_H */
