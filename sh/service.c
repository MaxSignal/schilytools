/*
 * CDDL HEADER START
 *
 * The contents of this file are subject to the terms of the
 * Common Development and Distribution License (the "License").
 * You may not use this file except in compliance with the License.
 *
 * You can obtain a copy of the license at usr/src/OPENSOLARIS.LICENSE
 * or http://www.opensolaris.org/os/licensing.
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
 * Copyright 2008 Sun Microsystems, Inc.  All rights reserved.
 * Use is subject to license terms.
 */

/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

#pragma ident	"@(#)service.c	1.27	08/01/29 SMI"

#include "defs.h"

/*
 * This file contains modifications Copyright 2008-2009 J. Schilling
 *
 * @(#)service.c	1.14 09/07/11 2008-2009 J. Schilling
 */
#ifndef lint
static	UConst char sccsid[] =
	"@(#)service.c	1.14 09/07/11 2008-2009 J. Schilling";
#endif

/*
 * UNIX shell
 */

#include	<errno.h>
#include	<fcntl.h>
#include	"sh_policy.h"

#define	ARGMK	01

	short		initio	__PR((struct ionod *iop, int save));
	unsigned char *simple	__PR((unsigned char *s));
	unsigned char *getpath	__PR((unsigned char *s));
	int		pathopen __PR((unsigned char *path, unsigned char *name));
	unsigned char *catpath	__PR((unsigned char *path, unsigned char *name));
	unsigned char *nextpath	__PR((unsigned char *path));
	void		execa	__PR((unsigned char *at[], short pos));
static unsigned char *execs	__PR((unsigned char *ap, unsigned char *t[]));
	void		trim	__PR((unsigned char *at));
	void		trims	__PR((unsigned char *at));
	unsigned char *mactrim	__PR((unsigned char *at));
	unsigned char **scan	__PR((int argn));
static void		gsort	__PR((unsigned char *from[], unsigned char * to[]));
	int 		getarg	__PR((struct comnod *ac));
static int		split	__PR((unsigned char *s));

extern short topfd;


/*
 * service routines for `execute'
 */
short
initio(iop, save)
	struct ionod	*iop;
	int		save;
{
	unsigned char	*ion;
	int	iof, fd = -1;
	int		ioufd;
	short	lastfd;
	int	newmode;

	lastfd = topfd;
	while (iop) {
		iof = iop->iofile;
		ion = mactrim((unsigned char *)iop->ioname);
		ioufd = iof & IOUFD;

		if (*ion && (flags&noexec) == 0) {
			if (save) {
				fdmap[topfd].org_fd = ioufd;
				fdmap[topfd++].dup_fd = savefd(ioufd);
			}

			if (iof & IODOC_SUBST) {
				struct tempblk tb;

				subst(chkopen(ion, 0), (fd = tmpfil(&tb)));

				/*
				 * pushed in tmpfil() --
				 * bug fix for problem with
				 * in-line scripts
				 */
				poptemp();

				fd = chkopen(tmpout, 0);
				unlink((const char *)tmpout);
			} else if (iof & IOMOV) {
				if (eq(minus, ion)) {
					fd = -1;
					close(ioufd);
				} else if ((fd = stoi(ion)) >= USERIO) {
					failed(ion, badfile);
				}
				else
					fd = dup(fd);
			} else if (((iof & IOPUT) == 0) && ((iof & IORDW) == 0))
				fd = chkopen(ion, 0);
			else if (iof & IORDW) /* For <> */ {
				newmode = O_RDWR|O_CREAT;
				fd = chkopen(ion, newmode);
			} else if (flags & rshflg) {
				failed(ion, restricted);
			} else if (iof & IOAPP &&
			    (fd = open((char *)ion, 1)) >= 0) {
				lseek(fd, (off_t)0, SEEK_END);
			} else {
				fd = create(ion);
			}
			if (fd >= 0)
				renamef(fd, ioufd);
		}

		iop = iop->ionxt;
	}
	return (lastfd);
}

unsigned char *
simple(s)
unsigned char	*s;
{
	unsigned char	*sname;

	sname = s;
	while (1) {
		if (any('/', sname))
			while (*sname++ != '/')
				;
		else
			return (sname);
	}
}

unsigned char *
getpath(s)
	unsigned char	*s;
{
	unsigned char	*path, *newpath;
	int pathlen;

	if (any('/', s))
	{
		if (flags & rshflg) {
			failed(s, restricted);
			/* NOTREACHED */
		} else
			return ((unsigned char *)nullstr);
	} else if ((path = pathnod.namval) == 0)
		return ((unsigned char *)defpath);
	else {
		pathlen = length(path)-1;
		/* Add extra ':' if PATH variable ends in ':' */
		if (pathlen > 2 && path[pathlen - 1] == ':' &&
				path[pathlen - 2] != ':') {
			newpath = locstak();
			(void) memcpystak(newpath, path, pathlen);
			newpath[pathlen] = ':';
			endstak(newpath + pathlen + 1);
			return (newpath);
		} else
			return (cpystak(path));
	}
	return (NULL);		/* Not reached, but keeps GCC happy */
}

int
pathopen(path, name)
	unsigned char	*path;
	unsigned char	*name;
{
	int	f;

	do
	{
		path = catpath(path, name);
	} while ((f = open((char *)curstak(), 0)) < 0 && path);
	return (f);
}

unsigned char *
catpath(path, name)
	unsigned char	*path;
	unsigned char	*name;
{
	/*
	 * leaves result on top of stack
	 */
	unsigned char	*scanp = path;
	unsigned char	*argp = locstak();

	while (*scanp && *scanp != COLON) {
		if (argp >= brkend)
			growstak(argp);
		*argp++ = *scanp++;
	}
	if (scanp != path) {
		if (argp >= brkend)
			growstak(argp);
		*argp++ = '/';
	}
	if (*scanp == COLON)
		scanp++;
	path = (*scanp ? scanp : 0);
	scanp = name;
	do
	{
		if (argp >= brkend)
			growstak(argp);
	}
	while ((*argp++ = *scanp++) != '\0');
	return (path);
}

unsigned char *
nextpath(path)
	unsigned char	*path;
{
	unsigned char	*scanp = path;

	while (*scanp && *scanp != COLON)
		scanp++;

	if (*scanp == COLON)
		scanp++;

	return (*scanp ? scanp : 0);
}

static const char	*xecmsg;
static unsigned char	**xecenv;

#ifdef	PROTOTYPES
void
execa(unsigned char *at[], short pos)
#else
void
execa(at, pos)
	unsigned char	*at[];
	short		pos;
#endif
{
	unsigned char	*path;
	unsigned char	**t = at;
	int		cnt;

	if ((flags & noexec) == 0) {
		xecmsg = notfound;
		path = getpath(*t);
		xecenv = local_setenv();

		if (pos > 0) {
			cnt = 1;
			while (cnt != pos) {
				++cnt;
				path = nextpath(path);
			}
			execs(path, t);
			path = getpath(*t);
		}
		while ((path = execs(path, t)) != NULL)
			;
		failed(*t, xecmsg);
	}
}

static unsigned char *
execs(ap, t)
	unsigned char	*ap;
	unsigned char	*t[];
{
	int		pfstatus = NOATTRS;
	unsigned char	*p, *prefix;
#ifdef	EXECATTR_FILENAME
	unsigned char	*savptr;
#endif

	prefix = catpath(ap, t[0]);
	trim(p = curstak());
	sigchk();

#ifdef	EXECATTR_FILENAME
	if (flags & pfshflg) {
		/*
		 * Need to save the stack information, or the
		 * first memory allocation in secpolicy_profile_lookup()
		 * will clobber it.
		 */
		savptr = endstak(p + strlen((const char *)p) + 1);

		pfstatus = secpolicy_pfexec((const char *)p,
		    (char **)t, (const char **)xecenv);

		if (pfstatus != NOATTRS) {
			errno = pfstatus;
		}

		tdystak(savptr);
	}
#endif

	if (pfstatus == NOATTRS) {
		execve((const char *)p, (char *const *)&t[0],
		    (char *const *)xecenv);
	}

	switch (errno) {
	case ENOEXEC:		/* could be a shell script */
		funcnt = 0;
		flags = 0;
		*flagadr = 0;
		comdiv = 0;
		ioset = 0;
		clearup();	/* remove open files and for loop junk */
		if (input)
			close(input);
		input = chkopen(p, 0);

#ifdef ACCT
		preacct(p);	/* reset accounting */
#endif

		/*
		 * set up new args
		 */

		setargs(t);
		longjmp(subshell, 1);

	case ENOMEM:
		failed(p, toobig);

	case E2BIG:
		failed(p, arglist);

	case ETXTBSY:
		failed(p, txtbsy);

#ifdef	ELIBACC
	case ELIBACC:
		failed(p, libacc);
#endif

#ifdef	ELIBBAD
	case ELIBBAD:
		failed(p, libbad);
#endif

#ifdef	ELIBSCN
	case ELIBSCN:
		failed(p, libscn);
#endif

#ifdef	ELIBMAX
	case ELIBMAX:
		failed(p, libmax);
#endif

	default:
		xecmsg = badexec;
	case ENOENT:
		return (prefix);
	}
}

BOOL		nosubst;

void
trim(at)
	unsigned char	*at;
{
	unsigned char	*last;
	unsigned char 	*current;
	unsigned char	c;
	int	len;
	wchar_t	wc;

	nosubst = 0;
	if ((current = at) != NULL) {
		last = at;
		(void) mbtowc(NULL, NULL, 0);
		while ((c = *current) != 0) {
			if ((len = mbtowc(&wc, (char *)current,
					MB_LEN_MAX)) <= 0) {
				(void) mbtowc(NULL, NULL, 0);
				*last++ = c;
				current++;
				continue;
			}

			if (wc != '\\') {
				memcpy(last, current, len);
				last += len;
				current += len;
				continue;
			}

			/* remove \ and quoted nulls */
			nosubst = 1;
			current++;
			if ((c = *current) != 0) {
				if ((len = mbtowc(&wc, (char *)current,
						MB_LEN_MAX)) <= 0) {
					(void) mbtowc(NULL, NULL, 0);
					*last++ = c;
					current++;
					continue;
				}
				memcpy(last, current, len);
				last += len;
				current += len;
			} else
				current++;
		}

		*last = 0;
	}
}

/* Same as trim, but only removes backlashes before slashes */
void
trims(at)
unsigned char	*at;
{
	unsigned char	*last;
	unsigned char 	*current;
	unsigned char	c;
	int	len;
	wchar_t	wc;

	if ((current = at) != NULL)
	{
		last = at;
		(void) mbtowc(NULL, NULL, 0);
		while ((c = *current) != 0) {
			if ((len = mbtowc(&wc, (char *)current,
					MB_LEN_MAX)) <= 0) {
				(void) mbtowc(NULL, NULL, 0);
				*last++ = c;
				current++;
				continue;
			}

			if (wc != '\\') {
				memcpy(last, current, len);
				last += len; current += len;
				continue;
			}

			/* remove \ and quoted nulls */
			current++;
			if (!(c = *current)) {
				current++;
				continue;
			}

			if (c == '/') {
				*last++ = c;
				current++;
				continue;
			}

			*last++ = '\\';
			if ((len = mbtowc(&wc, (char *)current,
					MB_LEN_MAX)) <= 0) {
				(void) mbtowc(NULL, NULL, 0);
				*last++ = c;
				current++;
				continue;
			}
			memcpy(last, current, len);
			last += len; current += len;
		}
		*last = 0;
	}
}

unsigned char *
mactrim(s)
unsigned char	*s;
{
	unsigned char	*t = macro(s);

	trim(t);
	return (t);
}

unsigned char **
scan(argn)
int	argn;
{
	struct argnod *argp =
			(struct argnod *)(Rcheat(gchain) & ~ARGMK);
	unsigned char **comargn, **comargm;

	comargn = (unsigned char **)getstak((Intptr_t)BYTESPERWORD * argn + BYTESPERWORD);
	comargm = comargn += argn;
	*comargn = ENDARGS;
	while (argp)
	{
		*--comargn = argp->argval;

		trim(*comargn);
		argp = argp->argnxt;

		if (argp == 0 || Rcheat(argp) & ARGMK)
		{
			gsort(comargn, comargm);
			comargm = comargn;
		}
		argp = (struct argnod *)(Rcheat(argp) & ~ARGMK);
	}
	return (comargn);
}

static void
gsort(from, to)
unsigned char	*from[], *to[];
{
	int	k, m, n;
	int	i, j;

	if ((n = to - from) <= 1)
		return;
	for (j = 1; j <= n; j *= 2)
		;
	for (m = 2 * j - 1; m /= 2; )
	{
		k = n - m;
		for (j = 0; j < k; j++)
		{
			for (i = j; i >= 0; i -= m)
			{
				unsigned char **fromi;

				fromi = &from[i];
				if (cf(fromi[m], fromi[0]) > 0)
				{
					break;
				}
				else
				{
					unsigned char *s;

					s = fromi[m];
					fromi[m] = fromi[0];
					fromi[0] = s;
				}
			}
		}
	}
}

/*
 * Argument list generation
 */
int
getarg(ac)
struct comnod	*ac;
{
	struct argnod	*argp;
	int		count = 0;
	struct comnod	*c;

	if ((c = ac) != NULL)
	{
		argp = c->comarg;
		while (argp)
		{
			count += split(macro(argp->argval));
			argp = argp->argnxt;
		}
	}
	return (count);
}

static int
split(s)		/* blank interpretation routine */
unsigned char	*s;
{
	unsigned char	*argp;
	int		c;
	int		count = 0;

	(void) mbtowc(NULL, NULL, 0);
	for (;;)
	{
		int clength;
		sigchk();
		argp = locstak() + BYTESPERWORD;
		while ((c = *s) != 0) {
			wchar_t wc;
			if ((clength = mbtowc(&wc, (char *)s,
					MB_LEN_MAX)) <= 0) {
				(void) mbtowc(NULL, NULL, 0);
				wc = (unsigned char)*s;
				clength = 1;
			}

			if (c == '\\') { /* skip over quoted characters */
				if (argp >= brkend)
					growstak(argp);
				*argp++ = c;
				s++;
				/* get rest of multibyte character */
				if ((clength = mbtowc(&wc, (char *)s,
						MB_LEN_MAX)) <= 0) {
					(void) mbtowc(NULL, NULL, 0);
					wc = (unsigned char)*s;
					clength = 1;
				}
				if (argp >= brkend)
					growstak(argp);
				*argp++ = *s++;
				while (--clength > 0) {
					if (argp >= brkend)
						growstak(argp);
					*argp++ = *s++;
				}
				continue;
			}

			if (anys(s, ifsnod.namval)) {
				/* skip to next character position */
				s += clength;
				break;
			}

			if (argp >= brkend)
				growstak(argp);
			*argp++ = c;
			s++;
			while (--clength > 0) {
				if (argp >= brkend)
					growstak(argp);
				*argp++ = *s++;
			}
		}
		if (argp == staktop + BYTESPERWORD)
		{
			if (c)
			{
				continue;
			}
			else
			{
				return (count);
			}
		}
		/*
		 * file name generation
		 */

		argp = endstak(argp);
		trims(((struct argnod *)argp)->argval);
		if ((flags & nofngflg) == 0 &&
			(c = expand(((struct argnod *)argp)->argval, 0)))
			count += c;
		else
		{
			makearg((struct argnod *)argp);
			count++;
		}
		gchain = (struct argnod *)((Intptr_t)gchain | ARGMK);
	}
}

#ifdef ACCT
#include	<sys/types.h>
#include	<sys/acct.h>
#include 	<sys/times.h>

#ifdef	AHZV1		/* Newer FreeBSD versions */
struct acctv1 sabuf;
#else
struct acct sabuf;
#endif
struct tms buffer;
static clock_t before;
static int shaccton;	/* 0 implies do not write record on exit */
			/* 1 implies write acct record on exit */
	void suspacct	__PR((void));
	void preacct	__PR((unsigned char *cmdadrp));
	void doacct	__PR((void));
static comp_t compress	__PR((clock_t));


/*
 *	suspend accounting until turned on by preacct()
 */
void
suspacct()
{
	shaccton = 0;
}

void
preacct(cmdadrp)
	unsigned char	*cmdadrp;
{
	if (acctnod.namval && *acctnod.namval) {
		sabuf.ac_btime = time((time_t *)0);
		before = times(&buffer);
		sabuf.ac_uid = getuid();
		sabuf.ac_gid = getgid();
		movstrn(simple(cmdadrp), (unsigned char *)sabuf.ac_comm, sizeof (sabuf.ac_comm));
		shaccton = 1;
	}
}

void
doacct()
{
	int fd;
	clock_t after;

	if (shaccton) {
		after = times(&buffer);
		sabuf.ac_utime = compress(buffer.tms_utime + buffer.tms_cutime);
		sabuf.ac_stime = compress(buffer.tms_stime + buffer.tms_cstime);
		sabuf.ac_etime = compress(after - before);

		if ((fd = open((char *)acctnod.namval,
				O_WRONLY | O_APPEND | O_CREAT, 0666)) != -1) {
			write(fd, &sabuf, sizeof (sabuf));
			close(fd);
		}
	}
}

/*
 *	Produce a pseudo-floating point representation
 *	with 3 bits base-8 exponent, 13 bits fraction
 */

static comp_t
compress(t)
	clock_t	t;
{
	int exp = 0;
	int rund = 0;

	while (t >= 8192) {
		exp++;
		rund = t & 04;
		t >>= 3;
	}

	if (rund) {
		t++;
		if (t >= 8192) {
			t >>= 3;
			exp++;
		}
	}
	return ((exp << 13) + t);
}
#endif
