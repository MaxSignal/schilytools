/* @(#)pfexec.c	1.2 08/08/06 Copyright 2008 J. Schilling */
#ifndef lint
static	char sccsid[] =
	"@(#)pfexec.c	1.2 08/08/06 Copyright 2008 J. Schilling";
#endif
/*
 *	Profile support for /usr/bin/pfexec
 *
 *	Copyright (c) 2008 J. Schilling
 */
/*
 * The contents of this file are subject to the terms of the
 * Common Development and Distribution License, Version 1.0 only
 * (the "License").  You may not use this file except in compliance
 * with the License.
 *
 * See the file CDDL.Schily.txt in this distribution for details.
 *
 * When distributing Covered Code, include this CDDL HEADER in each
 * file and include the License file CDDL.Schily.txt from this distribution.
 */

#include <schily/mconfig.h>
#include <stdio.h>
#include <schily/dirent.h>
#include <schily/maxpath.h>
#include "bsh.h"
#include "str.h"
#include "strsubs.h"

#ifdef	EXECATTR_FILENAME

#define	PFEXEC		"/usr/bin/pfexec"

LOCAL	char	*uname;

EXPORT	void	pfinit		__PR((void));
EXPORT	int	pfexec		__PR((char **path, char *name, FILE *in, FILE *out, FILE *err, char **av, char **env));

#define	NOATTRS	0	/* command in profile but w'out attributes */

EXPORT void
pfinit()
{
	if (uname == NULL)
		uname = getuname(getuid());
	if (uname && uname[0] >= '0' && uname[0] <= '9') {
		berror("Cannot get passwd entry.\n");
		free(uname);
		uname = NULL;
	}
}

LOCAL int
pfrpath(pname, rpath)
	char	*pname;
	char	*rpath;
{
	char	*name;

	if (*pname != '/') {
		name = concat(getcurenv(cwdname), slash, pname, (char *)NULL);
	} else {
		name = pname;
	}
	if (realpath(name, rpath) == NULL) {
		if (*pname != '/')
			free(name);
		return (ENOENT);
	}
	if (*pname != '/')
		free(name);
	return (0);
}

LOCAL char **
pfnewargv(av, rpath)
	char	*av[];
	char	*rpath;
{
	int	i;
	int	args = 0;
	char	**ret;

	for (i = 0; av[i] != NULL; i++)
		args++;
	args += 2;		/* PFEXEC + NULL */

	if ((ret = malloc(args * sizeof (ret))) == NULL)
		return (ret);

	ret[0] = (char *)PFEXEC;
	args -= 2;
	for (i = 0; i < args; i++)
		ret[i+1] = av[i];
	ret[i+1] = NULL;
	return (ret);
}

EXPORT int
pfexec(path, name, in, out, err, av, env)
		char	**path;
	register char	*name;
		FILE	*in;
		FILE	*out;
		FILE	*err;
		char	*av[];
		char	*env[];
{
	int		ret;
	char		*pname;
	char		rpath[MAXPATHNAME + 1];
	char		**pfav;
	execattr_t	*exec;

	if (uname == NULL)
		return (NOATTRS);		/* Need to call exec() later */

	if ((pname = findinpath(name, X_OK, TRUE)) == NULL)
		return (ENOENT);

	ret = pfrpath(pname, rpath);
	free(pname);
	if (ret != 0)
		return (ret);

	if ((exec = getexecuser(uname, KV_COMMAND,
				(const char *)rpath, GET_ONE)) == NULL) {
		return (ENOENT);
	}
	if ((exec->attr != NULL) && (exec->attr->length != 0)) {
		free_execattr(exec);

		pfav = pfnewargv(av, rpath);
		if (pfav == NULL)
			return (ENOMEM);
		seterrno(0);
#ifdef	VFORK
		Vav = pfav;
#endif
		ret = fexec(path, pfav[0], in, out, err, pfav, env);
		free(pfav);
#ifdef	VFORK
		Vav = NULL;
#endif
		return (ret);
	} else {
		free_execattr(exec);
		return (NOATTRS);		/* Need to call exec() later */
	}
}

#else	/* EXECATTR_FILENAME */

EXPORT	void	pfinit		__PR((void));

EXPORT void
pfinit()
{
}
#endif	/* EXECATTR_FILENAME */
