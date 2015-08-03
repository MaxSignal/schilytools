/*
 * CDDL HEADER START
 *
 * The contents of this file are subject to the terms of the
 * Common Development and Distribution License, Version 1.0 only
 * (the "License").  You may not use this file except in compliance
 * with the License.
 *
 * You can obtain a copy of the license at usr/src/OPENSOLARIS.LICENSE
 * A copy of the CDDL is also available via the Internet at
 * http://www.opensource.org/licenses/cddl1.txt
 * See the License for the specific language governing permissions
 * and limitations under the License.
 *
 * When distributing Covered Code, include this CDDL HEADER in each
 * file and include the License file at usr/src/OPENSOLARIS.LICENSE.
 * If applicable, add the following below this CDDL HEADER, with the
 * fields enclosed by brackets "[]" replaced with your own identifying
 * information: Portions Copyright [yyyy] [name of copyright owner]
 *
 * CDDL HEADER END
 */

/*
 * Copyright 2006 Sun Microsystems, Inc.  All rights reserved.
 * Use is subject to license terms.
 */

/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

#if defined(sun)
#pragma ident	"@(#)test.c	1.17	06/06/20 SMI"
#endif

#include "defs.h"

/*
 * Copyright 2008-2015 J. Schilling
 *
 * @(#)test.c	1.19 15/07/25 2008-2015 J. Schilling
 */
#ifndef lint
static	UConst char sccsid[] =
	"@(#)test.c	1.19 15/07/25 2008-2015 J. Schilling";
#endif


/*
 *      test expression
 *      [ expression ]
 */

#ifdef	SCHILY_INCLUDES
#include	<schily/types.h>
#include	<schily/stat.h>
#else
#include <sys/types.h>
#include <sys/stat.h>
#endif

#ifndef	HAVE_LSTAT
#define	lstat	stat
#endif
#ifndef	HAVE_DECL_STAT
extern int stat	__PR((const char *, struct stat *));
#endif
#ifndef	HAVE_DECL_LSTAT
extern int lstat __PR((const char *, struct stat *));
#endif

#define	exp	_exp	/* Some compilers do not like exp() */

	int	test		__PR((int argn, unsigned char *com[]));
#ifdef	DO_SYSATEXPR
	void	expr		__PR((int argn, unsigned char *com[]));
static	long	aexpr		__PR((struct namnod *n,
						unsigned char *op, long y));
#endif
static	unsigned char *nxtarg	__PR((int mt));
static	int	exp		__PR((void));
static	int	e1		__PR((void));
static	int	e2		__PR((void));
static	int	e3		__PR((void));
static	int	ftype		__PR((unsigned char *f, int field));
static	int	filtyp		__PR((unsigned char *f, int field));
static	int	fsizep		__PR((unsigned char *f));
static	Intmax_t str2imax	__PR((unsigned char *a));

static int	isexpr;
static int	ap, ac;
static unsigned char **av;

int
test(argn, com)
	int		argn;
	unsigned char	*com[];
{
	ac = argn;
	av = com;
	ap = 1;
	isexpr = 0;

	if (eq(com[0], "[")) {
		if (!eq(com[--ac], "]"))
			failed((unsigned char *)"test", nobracket);
	}
	com[ac] = 0;
	if (ac <= 1)
		return (1);
	return (exp() ? 0 : 1);
}

#ifdef	DO_SYSATEXPR
void
expr(argn, com)
	int		argn;
	unsigned char	*com[];
{
	int		incr = 0;
	struct namnod	*n = NULL;		/* Make GCC happy */
	char		buf[40];

	ac = argn;
	av = com;
	ap = 1;
	isexpr = 1;

	if (ac == 2) {				/* @ var++ */
		int len = length(av[ap]);

		if (len > 3) {
			unsigned char	*p = av[ap] + len - 3;

			if (eq(p, "++"))
				incr = 1;
			else if (eq(p, "--"))
				incr = -1;
			if (incr) {
				*p = '\0';
				n = lookup(av[ap]);
			}
		}
		if (incr == 0)
			failed((unsigned char *)"@", noarg);
	} else if (ac < 4) {
		failed((unsigned char *)"@", noarg);
	}
	if (incr) {
		snprintf(buf, sizeof (buf), "%ld", aexpr(n, UC "+=", incr));
	} else {
		n = lookup(av[ap++]);
		snprintf(buf, sizeof (buf), "%ld", aexpr(n, av[ap++], exp()));
	}
	assign(n, UC buf);
}

static long
aexpr(n, op, y)
	struct namnod	*n;
	unsigned char	*op;
	long		y;
{
	long		x;
	char		c = *op;

	if (eq(op, "="))
		return (y);

	if (c == 0 || op[1] != '=' || op[2])
		bfailed((unsigned char *)"@", badop, op);

	if (n->namval == NULL)
		failed((unsigned char *)"@", unset);
	x = str2imax(n->namval);

	switch (c) {

	case '+':	return (x + y);
	case '-':	return (x - y);
	case '*':	return (x * y);
	case '/':	return (x / y);
	case '%':	return (x % y);

	default:
		bfailed((unsigned char *)"@", badop, op);
	}

	return (-1);
}
#endif

static unsigned char *
nxtarg(mt)
	int	mt;
{
	if (ap >= ac)
	{
		if (mt)
		{
			ap++;
			return (0);
		}
		failed((unsigned char *)"test", noarg);
	}
	return (av[ap++]);
}

static int
exp()
{
	int	p1;
	unsigned char	*p2;

	p1 = e1();
	p2 = nxtarg(1);
	if (p2 != 0) {
		if (eq(p2, "-o"))
			return (p1 | exp());

#ifdef	__nono__
		if (!eq(p2, ")"))
			failed((unsigned char *)"test", synmsg);
#endif
	}
	ap--;
	return (p1);
}

static int
e1()
{
	int	p1;
	unsigned char	*p2;

	p1 = e2();
	p2 = nxtarg(1);

	if ((p2 != 0) && eq(p2, "-a"))
		return (p1 & e1());
	ap--;
	return (p1);
}

static int
e2()
{
	if (eq(nxtarg(0), "!"))
		return (!e3());
	ap--;
	return (e3());
}

static int
e3()
{
	int	p1;
	unsigned char	*a;
	unsigned char	*p2;
	Intmax_t		ll_1, ll_2;

	a = nxtarg(0);
	if (eq(a, "(")) {
		p1 = exp();
		if (!eq(nxtarg(0), ")"))
			failed((unsigned char *)"test", noparen);
		return (p1);
	}
	p2 = nxtarg(1);
	ap--;
	if ((p2 == 0) || (!eq(p2, "=") && !eq(p2, "!="))) {
		if (eq(a, "-r"))
			return (chk_access(nxtarg(0), S_IREAD, 0) == 0);
		if (eq(a, "-w"))
			return (chk_access(nxtarg(0), S_IWRITE, 0) == 0);
		if (eq(a, "-x"))
			return (chk_access(nxtarg(0), S_IEXEC, 0) == 0);
		if (eq(a, "-d"))
			return (filtyp(nxtarg(0), S_IFDIR));
		if (eq(a, "-c"))
			return (filtyp(nxtarg(0), S_IFCHR));
		if (eq(a, "-b"))
			return (filtyp(nxtarg(0), S_IFBLK));
		if (eq(a, "-f")) {
			if (ucb_builtins) {
				struct stat statb;

				return (stat((char *)nxtarg(0), &statb) >= 0 &&
					(statb.st_mode & S_IFMT) != S_IFDIR);
			}
			else
				return (filtyp(nxtarg(0), S_IFREG));
		}
		if (eq(a, "-u"))
			return (ftype(nxtarg(0), S_ISUID));
		if (eq(a, "-g"))
			return (ftype(nxtarg(0), S_ISGID));
		if (eq(a, "-k"))
			return (ftype(nxtarg(0), S_ISVTX));
		if (eq(a, "-p"))
			return (filtyp(nxtarg(0), S_IFIFO));
		if (eq(a, "-h") || eq(a, "-L"))
			return (filtyp(nxtarg(0), S_IFLNK));
		if (eq(a, "-s"))
			return (fsizep(nxtarg(0)));
		if (eq(a, "-t")) {
			if (ap >= ac)		/* no args */
				return (isatty(1));
			else if (eq((a = nxtarg(0)), "-a") || eq(a, "-o")) {
				ap--;
				return (isatty(1));
			} else
				return (isatty(atoi((char *)a)));
		}
		if (eq(a, "-n"))
			return (!eq(nxtarg(0), ""));
		if (eq(a, "-z"))
			return (eq(nxtarg(0), ""));
	}

	p2 = nxtarg(1);
	if (p2 == 0) {
#ifdef	DO_SYSATEXPR
		if (isexpr && digit(*a)) {
			ll_1 = str2imax(a);
			return (ll_1);
		}
#endif
		return (!eq(a, ""));
	}
	if (eq(p2, "-a") || eq(p2, "-o")) {
		ap--;
		return (!eq(a, ""));
	}
	if (eq(p2, "="))
		return (eq(nxtarg(0), a));
	if (eq(p2, "!="))
		return (!eq(nxtarg(0), a));
	ll_1 = str2imax(a);
	ll_2 = str2imax(nxtarg(0));
	if (eq(p2, "-eq"))
		return (ll_1 == ll_2);
	if (eq(p2, "-ne"))
		return (ll_1 != ll_2);
	if (eq(p2, "-gt"))
		return (ll_1 > ll_2);
	if (eq(p2, "-lt"))
		return (ll_1 < ll_2);
	if (eq(p2, "-ge"))
		return (ll_1 >= ll_2);
	if (eq(p2, "-le"))
		return (ll_1 <= ll_2);

#ifdef	DO_SYSATEXPR
	if (isexpr) {
		char	c = *p2;

		if (c && p2[1] == '\0') {
			if (c == '+')
				return (ll_1 + ll_2);
			if (c == '-')
				return (ll_1 - ll_2);
			if (c == '*')
				return (ll_1 * ll_2);
			if (c == '/')
				return (ll_1 / ll_2);
			if (c == '%')
				return (ll_1 % ll_2);
			if (c == '&')
				return (ll_1 & ll_2);
			if (c == '|')
				return (ll_1 | ll_2);
			if (c == '>')
				return (ll_1 > ll_2);
			if (c == '<')
				return (ll_1 < ll_2);
		}
		if (eq(p2, "&&"))
			return (ll_1 && ll_2);
		if (eq(p2, "||"))
			return (ll_1 || ll_2);
		if (eq(p2, ">="))
			return (ll_1 >= ll_2);
		if (eq(p2, "<="))
			return (ll_1 <= ll_2);
		if (eq(p2, ">>"))
			return (ll_1 >> ll_2);
		if (eq(p2, "<<"))
			return (ll_1 << ll_2);
	}
#endif

	bfailed((unsigned char *)btest, badop, p2);
	/* NOTREACHED */

	return (0);		/* Not reached, but keeps GCC happy */
}

static int
ftype(f, field)
	unsigned char	*f;
	int		field;
{
	struct stat statb;

	if (stat((char *)f, &statb) < 0)
		return (0);
	if ((statb.st_mode & field) == field)
		return (1);
	return (0);
}

static int
filtyp(f, field)
	unsigned char	*f;
	int		field;
{
	struct stat statb;
	int (*statf) __PR((const char *_nm, struct stat *_fs)) =
					(field == S_IFLNK) ? lstat : stat;

	if ((*statf)((char *)f, &statb) < 0)
		return (0);
	if ((statb.st_mode & S_IFMT) == field)
		return (1);
	else
		return (0);
}


static int
fsizep(f)
	unsigned char	*f;
{
	struct stat statb;

	if (stat((char *)f, &statb) < 0)
		return (0);
	return (statb.st_size > 0);
}

static Intmax_t
str2imax(a)
	unsigned char	*a;
{
	Intmax_t	i;
#ifdef	HAVE_STRTOLL
	i = strtoll((char *)a, NULL, 10);
#else
	i = strtol((char *)a, NULL, 10);
#endif
	return (i);
}
