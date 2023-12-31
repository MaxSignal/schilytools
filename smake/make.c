/* @(#)make.c	1.228 21/08/20 Copyright 1985, 87, 88, 91, 1995-2021 J. Schilling */
#include <schily/mconfig.h>
#ifndef lint
static	UConst char sccsid[] =
	"@(#)make.c	1.228 21/08/20 Copyright 1985, 87, 88, 91, 1995-2021 J. Schilling";
#endif
/*
 *	Make program
 *
 *	Copyright (c) 1985, 87, 88, 91, 1995-2021 by J. Schilling
 *	Copyright (c) 2022--2023 by the schilytools team
 */
/*
 * The contents of this file are subject to the terms of the
 * Common Development and Distribution License, Version 1.0 only
 * (the "License").  You may not use this file except in compliance
 * with the License.
 *
 * See the file CDDL.Schily.txt in this distribution for details.
 * A copy of the CDDL is also available via the Internet at
 * http://www.opensource.org/licenses/cddl1.txt
 *
 * When distributing Covered Code, include this CDDL HEADER in each
 * file and include the License file CDDL.Schily.txt from this distribution.
 */

#include <schily/stdio.h>
#include <schily/standard.h>
#include <schily/getargs.h>
#include <schily/errno.h>
#include <schily/fcntl.h>
#include <schily/stdlib.h>
#include <schily/string.h>
#include <schily/unistd.h>
#include <schily/time.h>
#include <schily/signal.h>

#include <schily/dirent.h>
#include <schily/maxpath.h>
#include <schily/getcwd.h>
#ifdef  USE_NLS
#define	GT_COMERR		/* #define comerr gtcomerr */
#define	GT_ERROR		/* #define error gterror   */
#else
#define	NO_NLS
#endif
#include <schily/schily.h>
#include <schily/libport.h>
#include <schily/utime.h>
#include <schily/nlsdefs.h>

#include "make.h"
#include "job.h"
#include "version.h"

#ifndef  USE_NLS
#undef	gtprintf
#define	gtprintf	printf
#endif

char	make_version[] = VERSION;

#ifdef	NO_DEFAULTS_PATH
#undef	DEFAULTS_PATH
#define	DEFAULTS_PATH_SEARCH_FIRST
#endif

#ifdef	_FASCII
LOCAL	void	setup_env	__PR((void));
#endif
EXPORT	void	usage		__PR((int exitcode));
LOCAL	void	initmakefiles	__PR((void));
LOCAL	void	initvflagmacros	__PR((void));
LOCAL	int	dochdir		__PR((char *name));
LOCAL	int	addmakefile	__PR((char *name));
LOCAL	void	read_defs	__PR((void));
LOCAL	void	read_makefiles	__PR((void));
EXPORT	void	setup_dotvars	__PR((void));
LOCAL	void	setup_vars	__PR((void));
LOCAL	void	setup_MAKE	__PR((char *name));
EXPORT	char	*searchtype	__PR((int mode));
LOCAL	void	printdirs	__PR((void));
LOCAL	void	printvflagmacros	__PR((void));
LOCAL	int	addcommandline	__PR((char *  name));
LOCAL	void	addcmdlinedef	__PR((char *  name));
LOCAL	int	addvflagmacro	__PR((char *  name));
LOCAL	void	scan_cmdline	__PR((int ac, char **av));
LOCAL	void	read_cmdline	__PR((void));
EXPORT	void	doexport	__PR((char *));
EXPORT	void	dounexport	__PR((char *));
LOCAL	void	read_environ	__PR((void));
EXPORT	int	main		__PR((int ac, char ** av));
LOCAL	void	check_old_makefiles __PR((void));
LOCAL	void	getmakeflags	__PR((void));
LOCAL	void	read_makemacs	__PR((void));
LOCAL	char	*nextmakemac	__PR((char *s));
LOCAL	BOOL	read_mac	__PR((char *mf));
LOCAL	void	setmakeflags	__PR((void));
LOCAL	char	*stripmacros	__PR((char *macbase, char *new));
LOCAL	void	setmakeenv	__PR((char *envbase, char *envp));
EXPORT	BOOL	move_tgt	__PR((obj_t * from));
LOCAL	int	copy_file	__PR((char * from, char * objname));
EXPORT	BOOL	touch_file	__PR((char * name));
LOCAL	date_t	gcurtime	__PR((void));
LOCAL	date_t	gnewtime	__PR((void));
EXPORT	date_t	gftime		__PR((char * file));
LOCAL	BOOL	isdir		__PR((char * file));
EXPORT	Llong	gfileid		__PR((char * file));
EXPORT	char	*prtime		__PR((date_t  date));
LOCAL	void	handler		__PR((int signo));
LOCAL	void	exhandler	__PR((int excode, void *arg));
EXPORT	char	*curwdir	__PR((void));
LOCAL	char	*getdefaultsfile	__PR((void));
LOCAL	int	put_env		__PR((char *new));
LOCAL	int	unset_env	__PR((char *name));

BOOL	posixmode	= FALSE;	/* We found a .POSIX target	*/
BOOL	Aflag		= FALSE;	/* -a Do not run setup_arch()	*/
BOOL	Eflag		= FALSE;	/* -e Environment overrides vars*/
BOOL	Iflag		= FALSE;	/* -i Ignore command errors	*/
BOOL	Kflag		= FALSE;	/* -k Continue on unrelated tgts */
BOOL	Stopflag	= FALSE;	/* -S Stop on make errors 	*/
BOOL	NSflag		= FALSE;	/* -N Ignore no Source on dep.	*/
BOOL	Nflag		= FALSE;	/* -n Only show what to do	*/
BOOL	Qflag		= FALSE;	/* -q If up to date exit (0)	*/
BOOL	Rflag		= FALSE;	/* -r Turn off internal rules	*/
BOOL	Sflag		= FALSE;	/* -s Be silent			*/
BOOL	Tflag		= FALSE;	/* -t Touch objects		*/
int	Mlevel		= 0;		/* MAKE_LEVEL from environment	*/
int	Debug		= 0;		/* -d Print reason for rebuild	*/
int	XDebug		= 0;		/* -xd Print extended debug info*/
BOOL	Prdep		= FALSE;	/* -xM Print include dependency	*/
BOOL	Pr_obj		= FALSE;	/* -probj   Print object tree	*/
BOOL	Print		= FALSE;	/* -p Print macro/target definitions*/
int	Dmake		= 0;		/* -D Display makefile		*/
BOOL	help		= FALSE;	/* -help    Show Usage		*/
BOOL	pversion	= FALSE;	/* -version Show version string	*/
BOOL	No_Warn		= FALSE;	/* -w No warnings		*/
int	Do_Warn		= 0;		/* -W Print extra warnings	*/
char	Makeflags[]	= "MAKEFLAGS";
char	Make_Flags[]	= "MAKE_FLAGS";
char	Make_Macs[]	= "MAKE_MACS";
char	Make_Level[]	= "MAKE_LEVEL";
char	Envdefs[]	= "Environment defs";
char	Makedefs[]	= "Internal Makefile";
char	Ldefaults[]	= "defaults.smk";
#ifdef	SVR4
char	Defaults[]	= "/opt/schily/share/lib/smake/defaults.smk";
#else
char	Defaults[]	= "/usr/bert/share/lib/smake/defaults.smk";
#endif
#define	MAKEFILECOUNT	32		/* Max number of Makefiles	*/
char	SMakefile[]	= "SMakefile";	/* smake's default Makefile	*/
char	Makefile[]	= "Makefile";	/* Primary default Makefile	*/
char	_makefile[]	= "makefile";	/* Secondary default Makefile	*/
char   **MakeFileNames;			/* To hold all Makefilenames	*/
int	Mfileindex;			/* Current Makefile index	*/
int	Mfilesize;			/* Size of Makefile array	*/
int	Mfilecount;			/* Number of Makefiles found+2	*/
char	CmdLMac[]	= "Command Line Macro"; /* Makefile Name for ..	*/
char	**CmdLDefs;			/* To hold all Cmdline Macros	*/
int	Cmdlinecount;			/* Number of Cmdline Macros	*/
int	Cmdlinesize;			/* Size of Cmdline Macro array	*/
char	*MFCmdline;			/* Pointer to Cmdl. Macs fr. env*/
char	**VflagMacros;			/* List of macros to expand and print	*/
int	VflagMacrossize;		/* Size of macros array. if
					 * non-zero, don't make
					 * anything and only print
					 * values of the macros.	*/

int	Mflags;

char	*ObjDir;		/* .OBJDIR: pathname Target destination dir */
int	ObjDirlen;		/* strlen(.OBJDIR)			    */
Llong	ObjDirfid;		/* gfileid(.OBJDIR)			    */
Llong	dotfid;			/* gfileid(".")				    */
int	ObjSearch	= SALL;	/* .OBJSEARCH: searchtype for explicit rules*/
list_t	*SearchList;		/* .SEARCHLIST: list of src/obj dir pairs   */
list_t	*Suffixes;		/* .SUFFIXES: list of suffixes (POSIX)	    */
BOOL	SSuffrules;
obj_t	*Init;			/* .INIT: command to execute at startup	    */
obj_t	*Done;			/* .DONE: command do execute on success	    */
obj_t	*Failed;		/* .FAILED: command to execute on failure   */
obj_t	*IncludeFailed;		/* .INCLUDE_FAILED: cmd to exec if missing  */
obj_t	*Deflt;			/* .DEFAULT: command to execute if no rule  */
obj_t	*Precious;		/* .PRECIOUS: list of targets not to remove */
obj_t	*Phony;			/* .PHONY: list of false targets, no check  */
obj_t	*curtarget;		/* current target of actually running cmd   */
date_t	curtime;		/* Current time				    */
date_t	newtime;		/* Special time newer than all		    */

#define	MINSECS		60
#define	HOURSECS	(MINSECS*60)
#define	DAYSECS		(HOURSECS*24)
#define	MONTHSECS	(DAYSECS*30)
#define	YEARSECS	(DAYSECS*365)
#ifdef	USE_NSECS
#define	DATESECS	((date_t)(1 << NSEC_SHIFT))
#else
#define	DATESECS	((date_t)1)
#endif

char	Nullstr[]	= "";
char	slash[]		= PATH_DELIM_STR;
int	slashlen	= sizeof (PATH_DELIM_STR) -1;	/* strlen(slash) */

LOCAL	BOOL	xpatrules;	/* Have non local pattern rules	in Makefile */
EXPORT	int	xssrules;	/* Have non local simple suffix rules	    */

#ifdef	_FASCII				/* Mark Williams C		*/
char	*_stksize	= (char *) 8192;

/*
 * Old and probably outdated setup that was needed in 1986 to make
 * 'smake' run on a ATARI ST using the Marc Williams C development tools.
 */
LOCAL void
setup_env()
{
	register char	*ep;
	extern	 char	*getenv();

	if ((ep = getenv("PATH")) == (char *) NULL || *ep == '\0')
		putenv("PATH=.bin,,\\bin,\\lib");

	if ((ep = getenv("SUFF")) == (char *) NULL || *ep == '\0')
		putenv("SUFF=,.prg,.tos,.ttp");

	if ((ep = getenv("LIBPATH")) == (char *) NULL || *ep == '\0')
		putenv("LIBPATH=\\lib,\\bin");

	if ((ep = getenv("TMPDIR")) == (char *) NULL || *ep == '\0')
		putenv("TMPDIR=\\tmp");

	if ((ep = getenv("INCDIR")) == (char *) NULL || *ep == '\0')
		putenv("INCDIR=\\include");
}
#endif

EXPORT void
usage(exitcode)
	int	exitcode;
{
	error("Usage:	smake [options] [target...] [macro=value...]\n");
	error("Options:\n");
	error("	-a	Do not set up architecture specific make macros.\n");
	error("	-e	Environment overrides variables in Makefile.\n");
	error("	-i	Ignore errors from commands.\n");
	error("	-j maxj	Specify the number of jobs to run simultaneously.\n");
	error("	-k	Ignore target errors, continue on unrelated targets.\n");
	error("	-N	Continue if no source for nonexistent dependencies is found.\n");
	error("	-n	Don't make - only say what to do.\n");
	error("	-p	Print all macro and target definitions.\n");
	error("	-q	Question mode. Exit code is 0 if target is up to date.\n");
	error("	-r	Turn off internal rules.\n");
	error("	-s	Be silent.\n");
	error("	-S	Undo the effect of the -k option, terminate on target errors.\n");
	error("	-t	Touch Objects instead of executing defined commands.\n");
	error("	-V m	Fully expand and print the content of macro m. Can be specified mulitple times.\n");
	error("	-w	Don't print warning Messages.\n");
	error("	-W	Print extra (debug) warning Messages.\n");
	error("	-WW	Print even more (debug) warning Messages.\n");
	error("	-D	Display Makefiles as read in.\n");
	error("	-DD	Display Makefiles/Rules as read in.\n");
	error("	-d	Print reason why a target has to be rebuilt.\n");
	error("	-dd	Debug dependency check.\n");
	error("	-xM	Print include dependency.\n");
	error("	-xd	Print extended debug info.\n");
	error("	-probj	Print object tree.\n");
	error("	-help	Print this help.\n");
	error("	-version Print version number.\n");
	error("	-posix	Force POSIX behaviour.\n");
	error("	-C dir	Change directory to 'dir' before starting work.\n");
	error("	mf=makefilename | -f makefilename\n");
	error("More than one -f makefile option may be specified.\n");
	exit(exitcode);
}

LOCAL void
initmakefiles()
{
	/*
	 * Mfilecount	als Index fuer MakeFileNames[] bei addmakefile()
	 * Mfileindex	als globale Index Variable fuer den Parser
	 *
	 * This code needs to be kept in sync with the MF_IDX_* #defines
	 * in make.h
	 */
	Mfilecount = 0;
	addmakefile(Makedefs);		/* Implicit rules		*/
	addmakefile(Envdefs);		/* Environment strings		*/
	addmakefile(Makefile);		/* Default make file		*/
	Mfilecount--;			/* -f name overwrites Makefile	*/
}

LOCAL void
initvflagmacros()
{
    VflagMacrossize = 0;
    VflagMacros = NULL;
}

/*
 * Called by getargs() if a -C option was found.
 */
LOCAL int
dochdir(name)
	char	*name;
{
	if (chdir(name) < 0)
		comerr("Cannot change directory to '%s'\n", name);

	return (1);
}

/*
 * Add a new makefile to the list of makefiles.
 * Called by getargs() if a -f option was found.
 */
LOCAL int
addmakefile(name)
	char	*name;
{
	if (MakeFileNames == NULL) {
		/*
		 * Use a default size of 4 as we usually have 4 Makefiles.
		 */
		Mfilesize = 4;
		MakeFileNames = malloc(Mfilesize * sizeof (char *));
	} else if (Mfilesize <= (Mfilecount+1)) { /* One spare for CmdLMac */
		Mfilesize += 4;
		MakeFileNames = realloc(MakeFileNames, Mfilesize * sizeof (char *));
	}
	if (MakeFileNames == NULL)
		comerr("No memory for Makefiles.\n");

	MakeFileNames[Mfilecount++] = name;

	return (1);
}

/*
 * Add NAME to VFLAGMACROS array.
 * Called by getargs() if a -v option was found.
 */
LOCAL int
addvflagmacro(name)
	char	*name;
{
	VflagMacros = realloc(VflagMacros, (VflagMacrossize + 1) * sizeof (char *));
	if (VflagMacros == NULL)
		comerr("No memory for Vflag.\n");

	VflagMacros[VflagMacrossize++] = name;

	return (1);
}

/*
 * Read the default rules - either compiled in or from file.
 */
LOCAL void
read_defs()
{
		int	MFsave = Mfileindex;
		char	*deflts;
	extern	char	implicit_rules[]; /* Default rules compiled into make */

	Dmake--;
	Mfileindex = MF_IDX_IMPLICIT;	/* Index 0 Implicit Rules */

	if (gftime(Ldefaults) != 0) {
		MakeFileNames[0] = Ldefaults;
		readfile(Ldefaults, TRUE);
#ifdef	DEFAULTS_PATH_SEARCH_FIRST
	} else if ((deflts = getdefaultsfile()) != NULL) {
		MakeFileNames[0] = deflts;
		readfile(deflts, TRUE);
#endif
#ifdef	DEFAULTS_PATH				/* This is the install path */
	} else if (gftime(DEFAULTS_PATH) != 0) {
		MakeFileNames[0] = DEFAULTS_PATH;
		readfile(DEFAULTS_PATH, TRUE);
#endif
	} else if (gftime(Defaults) != 0) {
		MakeFileNames[0] = Defaults;
		readfile(Defaults, TRUE);
#ifndef	DEFAULTS_PATH_SEARCH_FIRST
	} else if ((deflts = getdefaultsfile()) != NULL) {
		MakeFileNames[0] = deflts;
		readfile(deflts, TRUE);
#endif
	} else {
		readstring(implicit_rules, Makedefs);
	}
	Dmake++;
	Mfileindex = MFsave;
}

/*
 * Read in all external makefiles. Use either the names from command line
 * or look for the default names "Makefile" and "makefile".
 */
LOCAL void
read_makefiles()
{
	int	MFsave = Mfileindex;
	patr_t	*oPatrules = Patrules;
	patr_t	**opattail = pattail;

	Patrules = 0;
	pattail = &Patrules;
	xssrules = 0;

	Mfileindex = MF_IDX_MAKEFILE;	/* Index 2 Default Makefile */
	if (Mfilecount == MF_IDX_MAKEFILE) {
		if (posixmode) {
			/*
			 * First look for "makefile"
			 * then for "Makefile", then for "SMakefile".
			 */
			if (gftime(_makefile) != 0) {		/* "makefile" */
				Mfilecount++;
				MakeFileNames[2] = _makefile;
			} else if (gftime(Makefile) != 0) {	/* "Makefile" */
				Mfilecount++;
				MakeFileNames[2] = Makefile;
			} else if (gftime(SMakefile) != 0) {	/* "SMakefile" */
				Mfilecount++;
				MakeFileNames[2] = SMakefile;
			}
		} else
		/*
		 * First look for "SMakefile",
		 * then for "Makefile", then for "makefile"
		 */
		if (gftime(SMakefile) != 0) {		/* "SMakefile" */
			Mfilecount++;
			MakeFileNames[2] = SMakefile;
		} else if (gftime(Makefile) != 0) {	/* "Makefile" */
			Mfilecount++;
			MakeFileNames[2] = Makefile;
		} else if (gftime(_makefile) != 0) {	/* "makefile" */
			Mfilecount++;
			MakeFileNames[2] = _makefile;
		}
	}
	while (Mfileindex < Mfilecount) {
		readfile(MakeFileNames[Mfileindex], TRUE);
		Mfileindex++;
	}

	/*
	 * Check for external pattern rule definitions.
	 */
	xpatrules = Patrules != NULL;
	/*
	 * The pattern rules which are defined in the external makefiles
	 * must supersede the pattern rules from the internal rules.
	 * Concat the pattern rules found in internal rules to the end of
	 * the patern rules list from external makefiles.
	 */
	*pattail = oPatrules;
	pattail = opattail;

	Mfileindex = MFsave;
}

/*
 * Setup special variables.
 *
 * NOTE: Must be reentrant because it may be called more than once
 *	 if the "include" directive is used.
 *	 As there is (currently) no way to delete an object
 *	 it is OK not to do anything special if objlook() returns NULL.
 */
EXPORT void
setup_dotvars()
{
	obj_t	*obj;
	list_t	*l;
	char	*name;

	obj = objlook(".OBJDIR", FALSE);
	if (obj != NULL && obj->o_type != ':')	/* Must be a special target */
		obj = NULL;

	if (obj != NULL) {
		if (obj->o_list && (ObjDir = obj->o_list->l_obj->o_name)) {
			if (gfileid(ObjDir) == gfileid(".")) {
				/*
				 * Do not allow moving targets to themselves.
				 */
				ObjDir = NULL;
				ObjDirlen = 0;
			} else {
				ObjDirlen = strlen(ObjDir);
			}
		}
	}
#ifdef	no_longer_needed	/* Has been moved to dynmac expansion */
	/*
	 * XXX We cannot do this here as we are called more than once and
	 * XXX we like to allow $O to be overwritten from Makefiles that
	 * XXX do not know about smake's special features.
	 */
	/*
	 * Create special variable $O -> ObjDir
	 */
	define_var("O", ObjDir ? ObjDir : ".");
#endif

	obj = objlook(".OBJSEARCH", FALSE);
	if (obj != NULL && obj->o_type != ':')	/* Must be a special target */
		obj = NULL;
	if (obj != NULL) {
		if ((l = obj->o_list) != NULL) {
			name = l->l_obj->o_name;
			if (streql("src", name)) {
				ObjSearch = SSRC;
			} else if (streql("obj", name)) {
				ObjSearch = SOBJ;
			} else if (streql("all", name)) {
				ObjSearch = SALL;
			} else {
				/*
				 * This is the default.
				 */
				ObjSearch = SALL;
			}
		}
	}

	obj = objlook(".SEARCHLIST", FALSE);
	if (obj != NULL && obj->o_type != ':')	/* Must be a special target */
		obj = NULL;
	if (obj != NULL) {
		SearchList = obj->o_list;
	} else if ((obj = objlook("VPATH", FALSE)) != NULL && obj->o_type == '=') {
		SearchList = cvtvpath(obj->o_list);
	}

	obj = objlook(".IGNORE", FALSE);
	if (obj != NULL && obj->o_type != ':')	/* Must be a special target */
		obj = NULL;
	if (obj != NULL && obj->o_list == NULL)
		Iflag = TRUE;

	obj = objlook(".SILENT", FALSE);
	if (obj != NULL && obj->o_type != ':')	/* Must be a special target */
		obj = NULL;
	if (obj != NULL && obj->o_list == NULL)
		Sflag = TRUE;

	Init = objlook(".INIT", FALSE);
	Done = objlook(".DONE", FALSE);
	Failed = objlook(".FAILED", FALSE);
	IncludeFailed = objlook(".INCLUDE_FAILED", FALSE);
	if (IncludeFailed != NULL && IncludeFailed->o_cmd == NULL)
		IncludeFailed = NULL;
	Deflt = objlook(".DEFAULT", FALSE);
	Precious = objlook(".PRECIOUS", FALSE);
	Phony = objlook(".PHONY", FALSE);

	if (objlook(".POSIX", FALSE))
		posixmode = TRUE;
	obj = objlook(".SUFFIXES", FALSE);
	if (obj != NULL && obj->o_type != ':')	/* Must be a special target */
		obj = NULL;
	if (obj != NULL)
		Suffixes = obj->o_list;
	if (Debug > 1 && Suffixes != (list_t *) NULL) {
		register list_t *p;

		error(".SUFFIXES :\t");
		for (p = Suffixes; p; p = p->l_next)
			error("%s ", p->l_obj->o_name);
		error("\n");
	}
	SSuffrules = check_ssufftab();
}

/*
 * Set up some known special macros.
 */
LOCAL void
setup_vars()
{
	int	i;
	char	make_level[64];
	char	*p;

	define_var("$", "$");			/* Really needed ? */
	define_var("NUMBER_SIGN", "#");		/* Allow to use '#' */
	define_var("MAKE_NAME", "smake");	/* Needed to identify syntax */
	define_var("MAKE_VERSION", make_version); /* Version dependant files? */
	if ((p = getenv(Make_Level)) != NULL) {
		p = astoi(p, &i);
		if (*p == '\0')
			Mlevel = i;
	}
	snprintf(make_level, sizeof (make_level), "%d", Mlevel+1);
	define_var(Make_Level, make_level);
	doexport(Make_Level);
}

/*
 * Set up the special macro $(MAKE).
 * If we were called with an absolute PATH or without any '/', use argv[0],
 * else compute the absolute PATH by prepending working dir to argv[0].
 * (We need to compute the absolute PATH anyway, so CURDIR will be set).
 */
LOCAL void
setup_MAKE(name)
	char	*name;
{
	char	wd[MAXPATHNAME + 1];
	int	len;

	/* curwdir() ensures CURDIR is populated */
	strncpy(wd, curwdir(), sizeof (wd));

	/*
	 * If argv[0] starts with a slash or contains no slash,
	 * or on DOS like OS starts with MS-DOS drive letter,
	 * it is useful as $(MAKE).
	 */
#ifdef HAVE_DOS_DRIVELETTER
	if (name[0] == SLASH || strchr(name, SLASH) == NULL || name[1] == ':') {
#else
	if (name[0] == SLASH || strchr(name, SLASH) == NULL) {
#endif
		define_var("MAKE", name);
	} else {
		/*
		 * Compute abs pathname for $(MAKE)
		 */
		wd[sizeof (wd)-1] = '\0';
		len = strlen(wd);
		if ((strlen(name) + len + 2) < sizeof (wd)) {
			strcat(wd, PATH_DELIM_STR);
			strcat(wd, name);
		}
		define_var("MAKE", wd);
	}
}

/*
 * Transfer object search types into human readable names.
 */
EXPORT char *
searchtype(mode)
	int	mode;
{
	if (mode == SSRC)
		return ("src");
	if (mode == SOBJ)
		return ("obj");
	if (mode == SALL)
		return ("all");
	return ("invalid Object search mode");
}

/*
 * Print some 'smake' special macros:
 * .OBJDIR, .SEARCHLIST and .OBJSEARCH
 */
LOCAL void
printdirs()
{
	if (ObjDir != NULL)
		error(".OBJDIR :\t%s\n", ObjDir);

	error(".OBJSEARCH :\t%s\n", searchtype(ObjSearch));

	if (SearchList != (list_t *) NULL) {
		register list_t *p;
			error(".SEARCHLIST :\t");
		for (p = SearchList; p; p = p->l_next)
			error("%s ", p->l_obj->o_name);
		error("\n");
	}
}

/*
 * Print the values of the macros listed in VflagMacros
 */
LOCAL void
printvflagmacros()
{
	int	i = 0;
	char	*expanded = NULL;

	if (VflagMacrossize <= 0 || VflagMacros == NULL)
		comerr("printvflagmacros called but VflagMacros array is empty");

	for (i = 0; i < VflagMacrossize; ++i) {
		/* Clear expanded as it might hold a value from
		 * previous iterations */
		expanded = NULL;

		/* Try to expand the given macro name */
		expanded = try_expand_name(VflagMacros[i]);

		/* If it failed but we it contains a $, try to expand
		 * it as an expression. */
		if (!expanded && (strchr(VflagMacros[i], '$') != NULL))
			expanded = substitute(VflagMacros[i], NullObj, NullObj, NULL);

		printf("%s\n", expanded ? expanded : "");
	}
}

/*
 * Check for a command line macro.
 * This is called by getargs().
 */
LOCAL int
addcommandline(name)
	char	*name;
{
	if (Debug > 1)
		error("got_it: %s\n", name);

/* UNIX make: ":;=$\n\t"	*/

	if (!strchr(name, '='))
		return (NOTAFILE); /* Tell getargs that this may be a flag */
	return (1);
}

/*
 * Add a command line macro to our list.
 * This is called by scan_cmdline() past getargs() since getargs() would
 * not permit spaces, '+' or ':' to the left of a '='.
 */
LOCAL void
addcmdlinedef(name)
	char	*name;
{
	if (CmdLDefs == NULL) {
		Cmdlinesize = 8;
		CmdLDefs = malloc(Cmdlinesize * sizeof (char *));
	} else if (Cmdlinesize <= Cmdlinecount) {
		Cmdlinesize += 8;
		CmdLDefs = realloc(CmdLDefs, Cmdlinesize * sizeof (char *));
	}
	if (CmdLDefs == NULL)
		comerr("No memory for Commandline Macros.\n");

	CmdLDefs[Cmdlinecount++] = name;
}

/*
 * Read in and parse all command line macro definitions from list.
 */
LOCAL void
scan_cmdline(ac, av)
	int	ac;
	char	**av;
{
	register int	i;

	for (i = 1; i < ac; i++) {
		if (av[i][0] == '-')
			continue;
		if (av[i-1][0] == '-' && strchr(av[i-1], 'V') != NULL)
			continue;
		if (strchr(av[i], '=') == NULL)
			continue;
		addcmdlinedef(av[i]);
	}
}

/*
 * Read in and parse all command line macro definitions from list.
 */
LOCAL void
read_cmdline()
{
		int	MFsave = Mfileindex;
	register int	i;

	if (Cmdlinecount == 0)
		return;

	/*
	 * Register the command line macros past the last makefile
	 */
	Mfileindex = Mfilecount;
	if (Mfileindex == MF_IDX_MAKEFILE)
		Mfileindex = MF_IDX_MAKEFILE + 1;
	MakeFileNames[Mfileindex] = CmdLMac;

	Mflags |= (F_READONLY|F_IDXOVERWRT);
	for (i = 0; i < Cmdlinecount; i++) {
		char	*eq;
		char	*sp;

		readstring(CmdLDefs[i], CmdLMac);

		eq = strchr(CmdLDefs[i], '=');
		sp = strchr(CmdLDefs[i], ' ');
		if ((sp == NULL || sp > eq) && eq[-1] != ':')
			put_env(CmdLDefs[i]);
	}
	Mflags &= ~(F_READONLY|F_IDXOVERWRT);
	Mfileindex = MFsave;
}

/*
 * Export a macro into the environment.
 * This is mainly done by the "export" directive inside a makefile.
 */
EXPORT void
doexport(oname)
	char	*oname;
{
	obj_t	*obj;
	list_t	*l;
	char	*name;
	int	len;

	obj = objlook(oname, FALSE);
	if (obj != NULL && basetype(obj->o_type) != '=') /* Must be a macro type target */
		obj = NULL;
	if (obj != NULL) {
		if ((l = obj->o_list) != NULL) {
			char	*xname;

			len = strlen(oname)+1;	/* Env name + '=' */
			while (l && l->l_obj->o_name) {
				xname = l->l_obj->o_name;
				if (gbuf != NULL)
					xname = substitute(xname,
								NullObj, 0, 0);
				len += strlen(xname)+1;
				l = l->l_next;
			}
			name = malloc(len);
			if (name == NULL)
				comerr("Cannot alloc memory for env.\n");
			name[0] = '\0';
			l = obj->o_list;
			strcat(name, oname);
			strcat(name, "=");
			while (l && l->l_obj->o_name) {
				xname = l->l_obj->o_name;
				if (gbuf != NULL)
					xname = substitute(xname,
								NullObj, 0, 0);
				strcat(name, xname);
				if (l->l_next == NULL)
					break;
				strcat(name, " ");
				l = l->l_next;
			}
			put_env(name);
		}
	}
}

/*
 * Unexport a macro from the environment.
 * This is mainly done by the "unexport" directive inside a makefile.
 */
EXPORT void
dounexport(oname)
	char	*oname;
{
	unset_env(oname);
}


/*
 * Read in and parse all environment vars to make them make macros.
 */
LOCAL void
read_environ()
{
		int	MFsave = Mfileindex;
	register char	**env;
	extern	char	**environ;
	char *ev;
	char *p;

	Dmake -= 2;
	Mfileindex = MF_IDX_ENVIRON;	/* Index 1 Environment vars */

	if (Eflag)
		Mflags |= F_READONLY;
	mfname = Envdefs;
	for (env = environ; *env; env++) {
		ev = *env;
		p = strchr(ev, EQUAL);
		if (p == NULL)
			continue;
		if (strncmp(ev, "CURDIR=", 7) == 0)
			continue;	/* Never import CURDIR */
		if (strncmp(ev, "SHELL=", 6) == 0)
			continue;	/* Never import SHELL */
		if (strncmp(ev, "FORCE_SHELL=", 12) == 0) {
			obj_t	*obj = objlook(".FORCE_SHELL", TRUE);
			if (obj->o_type == 0)
				obj->o_type = COLON;
		}
		*p = '\0';
		define_var(ev, &p[1]);
		*p = EQUAL;
	}
	mfname = NULL;
	Mflags &= ~F_READONLY;

	Dmake += 2;
	Mfileindex = MFsave;
}


EXPORT int
main(ac, av)
	int	ac;
	char	*av[];
{
		int	failures = 0;
		int	i;
		int	maxj = 0;
		int	cac = ac;
		char	* const *cav = av;
	static	char	options[] = "help,version,posix,a,e,i,j#,k,n,N,p,q,r,s,S,t,w,W+,d+,D+,xM,xd+,probj,C&,mf&,f&,V&,&";

	save_args(ac, av);

	(void) setlocale(LC_ALL, "");

#ifdef  USE_NLS
#if !defined(TEXT_DOMAIN)	/* Should be defined by cc -D */
#define	TEXT_DOMAIN "smake"	/* Use this only if it weren't */
#endif
	{ char	*dir;
	dir = searchfileinpath("share/locale", F_OK,
					SIP_ANY_FILE|SIP_NO_PATH, NULL);
	if (dir)
		(void) bindtextdomain(TEXT_DOMAIN, dir);
	else
#if defined(PROTOTYPES) && defined(INS_BASE)
	(void) bindtextdomain(TEXT_DOMAIN, INS_BASE "/share/locale");
#else
	(void) bindtextdomain(TEXT_DOMAIN, "/usr/share/locale");
#endif
	(void) textdomain(TEXT_DOMAIN);
	}
#endif 	/* USE_NLS */

#ifdef	__DJGPP__
	set_progname("smake");	/* We may have strange av[0] on DJGPP */
#endif

#ifdef	HAVE_GETPID
	getpid();		/* Give some info for truss(1) users */
#endif
#ifdef	HAVE_GETPGRP
	getpgrp();		/* Give some info for truss(1) users */
#endif

#ifdef	_FASCII			/* Mark Williams C	  */
	stderr->_ff &= ~_FSTBUF; /* setbuf was called ??? */

	setup_env();
#endif
	getmakeflags();		/* Default options from MAKEFLAGS=	*/
	initmakefiles();	/* Set up MakeFileNames[] array		*/
	initvflagmacros();	/* Set up VflagMacros[] array		*/

	cac--; cav++;
	if (getallargs(&cac, &cav, options, &help, &pversion, &posixmode,
			&Aflag,
			&Eflag, &Iflag, &maxj,
			&Kflag, &Nflag, &NSflag, &Print,
			&Qflag, &Rflag, &Sflag, &Stopflag, &Tflag,
			&No_Warn, &Do_Warn,
			&Debug, &Dmake, &Prdep, &XDebug, &Pr_obj,
			dochdir, NULL,
			addmakefile, NULL,
			addmakefile, NULL,
			addvflagmacro, NULL,
			addcommandline, NULL) < 0) {
		errmsgno(EX_BAD, "Bad flag: %s.\n", cav[0]);
		usage(EX_BAD);
	}
	if (help)
		usage(0);
	if (pversion) {
		gtprintf("Smake release %s %s (%s-%s-%s)\n",
				make_version, VERSION_DATE,
				HOST_CPU, HOST_VENDOR, HOST_OS);
		gtprintf("Copyright (C) 1985, 87, 88, 91, 1995-2021 %s\n"
		         "          (C) 2022-2023                   the schilytools team\n",
				_("J�rg Schilling"));
		exit(0);
	}
	if (maxj > 0 && maxj <= MAXJOBS_MAX)
		maxjobs = maxj;
	else if (maxj > 0)
		maxjobs = MAXJOBS_MAX;

	/*
	 * XXX Is this the right place to set the options and cmd line macros
	 * XXX to the exported environment?
	 * XXX Later in read_makemacs() we may find that MAKEFLAGS= may contain
	 * XXX garbage that has been propagated to MFCmdline.
	 * For this reason, we call read_makemacs() before, let it parse only
	 * and kill any unwanted content from MFCmdline.
	 */
	if (NullObj == 0)	/* First make sure we may expand vars	*/
		NullObj = objlook(Nullstr, TRUE);

	scan_cmdline(ac, av);	/* Identify and save cmd line macro defs */
	read_makemacs();	/* With gbuf == NULL, this is parse only */
	setmakeflags();
	if (Qflag) {
		Sflag = TRUE;
		Nflag = TRUE;
	}
	if (Tflag && !Nflag) {
		Nflag = -1;	/* Hack for touch_file() */
	}
	if (Stopflag) {
		Kflag = FALSE;
	}
	if (Debug > 0)
		error("MAKEFLAGS value: '%s'\n", getenv(Makeflags));

	/*
	 * XXX Reihenfolge bei UNIX make beachten!!!
	 */
	if (signal(SIGINT, SIG_IGN) != SIG_IGN)
		signal(SIGINT, handler);
#ifdef	SIGQUIT
	if (signal(SIGQUIT, SIG_IGN) != SIG_IGN)
		signal(SIGQUIT, handler);
#endif
#ifdef	SIGHUP
	if (signal(SIGHUP, SIG_IGN) != SIG_IGN)
		signal(SIGHUP, handler);
#endif
	if (signal(SIGTERM, SIG_IGN) != SIG_IGN)
		signal(SIGTERM, handler);
	curtime = gcurtime();
	newtime = gnewtime();
	NullObj->o_date = newtime;	/* Make NullObj "up to date"    */

	initchars();
	initgbuf(NAMEMAX);	/* Now the Makefile parser becomes usable */

	/*
	 * The functions read_cmdline() and read_makemacs() are setting
	 * the F_READONLY flag in all objects. The function read_environ()
	 * sets F_READONLY if the -e option was specified.
	 * This differs from the description in POSIX for 'make' but it
	 * is the only way to allow 'include' directives to work as expected.
	 * For the same reason, we are reading the macros in the opposite
	 * order as described in POSIX.
	 */
	read_cmdline();		/* First read cmd line macros		*/
	read_makemacs();	/* then the inherited cmd line macros	*/
	if (!Rflag)
		read_defs();	/* read "defaults.smk"			*/
	setup_MAKE(av[0]);	/* Set up $(MAKE)			*/
	setup_SHELL();		/* Set up $(SHELL)			*/
	setup_vars();		/* Set up some known special macros	*/
	if (!Aflag)
		setup_arch();	/* Set up arch specific macros		*/
	read_environ();		/* Sets F_READONLY if -e flag is present*/

	if (Debug > 0 && Mlevel > 0)
		error("Starting '%s'[%d] in directory   '%s'\n",
			av[0], Mlevel, curwdir());

	/*
	 * Clear default target, then look again in makefiles
	 */
	default_tgt = NULL;
	read_makefiles();

	/*
	 * Let all objects created later seem to bee in
	 * last Makefile or in implicit rules if no
	 * Makefile is present.
	 */
	Mfileindex = Mfilecount - 1;
	if (Mfileindex < MF_IDX_MAKEFILE)
		Mfileindex = MF_IDX_IMPLICIT;

	setup_dotvars();
	setup_xvars();		/* Set up compat vars like MAKE_SHELL_FLAG */
	if (!Rflag)
		check_old_makefiles();

	if (Pr_obj)		/* -probj Flag				*/
		printtree();
	if (Print) {		/* -p Flag				*/
		prtree();
		exit(0);	/* XXX Really exit() here for make -p -f /dev/null ??? */
				/* XXX Posix requires make -p -f /dev/null 2>/dev/null */
				/* XXX to print the internal macros	*/
	}
	on_comerr(exhandler, av[0]);
	if (Debug > 0)
		printdirs();	/* .OBJDIR .OBJSEARCH .SEARCHLIST	*/

	if (VflagMacrossize > 0) {	/* -V was specified at least once	*/
		printvflagmacros();
		exit(0);	/* Exit here because -V must not start building anything	*/
	}

	makeincs();		/* Re-make included files */
	omake(Init, TRUE);	/* Make .INIT target	  */
	cac = ac;
	cav = av;
	cac--; cav++;
	for (i = 0; getfiles(&cac, &cav, options); cac--, cav++, i++) {
		if (strchr(cav[0], '=')) { /* Skip command line macro defs */
			i--;
			continue;
		}
		if (!domake(cav[0]))	/* Make targets from command line */
			failures++;
	}

	if (i == 0 && !domake((char *) NULL))	/* Make default target */
		failures++;
	if (failures && Failed) {
		omake(Failed, TRUE);	/* Make .FAILED target */
	} else {
		omake(Done, TRUE);	/* Make .DONE target */
	}
#ifdef	DEBUG
	prmem();
#endif
	if (failures > 0)
		comexit(1);
	comexit(0);
	return (0);		/* keep lint happy :-) */
}

/*
 * Check for old makefile systems without pattern matching rules.
 *
 * Old makefile systems only define simple suffix rules. Now that newer smake
 * releases support POSIX suffix rules, the makefile system will only work
 * if the makefile system uses pattern matching rules.
 * We may remove this function in 2005.
 */
LOCAL void
check_old_makefiles()
{
	obj_t	*o;

	if (Suffixes == NULL)	/* No/Empty .SUFFIXES, no compat problems */
		return;
	if (xssrules == 0)	/* Makefile did not define simple suff rule*/
		return;
	if (xpatrules)		/* New makefiles define pattern rules	   */
		return;

	/*
	 * All makefiles from the Schily (SING) makefile system define
	 * _UNIQ=.XxZzy-
	 */
	if ((o = objlook("_UNIQ", FALSE)) == NULL)
		return;
	if (!streql(".XxZzy-", o->o_list->l_obj->o_name))
		return;

	errmsgno(EX_BAD, "WARNING: this project uses an old version of the makefile system.\n");
	comerrno(EX_BAD, "Update the makefiles or call 'smake -r'.\n");
}

/*
 * Read in and parse the makeflags we got from the MAKEFLAGS= environment var.
 * MAKEFLAGS= may be:
 *
 * -	"et..."			Only option letters.
 *				No space and no macro definitions.
 * -	"NAME=value..."		Only a list of macro definitions (like a
 *				make command line).
 * -	"-et..."		Options as they appear on the command line.
 *				Space and multiple '-' are allowed.
 *	"-et -- NAME=value..."	A complete make command line (except
 *				-f filename options and target arguments).
 * BSD make:
 *
 * -	" NAME=value..."	Only a list of macro definitions.
 *
 * -	" -e -t"		Only options.
 *
 * -	" -e -t NAME=value..."	A complete make command line.
 *
 * SunPro Make:
 *
 *	"-et  NAME=value..."	A complete make command line.
 */
LOCAL void
getmakeflags()
{
	int	MFsave = Mfileindex;
	char	*mf = getenv(Makeflags);
	BOOL	hasdash = FALSE;

	if (!mf || *mf == '\0')	/* No MAKEFLAGS= or empty MAKEFLAGS=	*/
		return;
	while (*mf == ' ')
		mf++;

	Mfileindex = MF_IDX_ENVIRON;	/* Index 1 Environment vars */
	if (*mf != '-') {	/* Only macros if does not start with '-'*/
		char *p = nextmakemac(mf);	/* Next unescaped ' '    */
		char *eql = strchr(mf, '=');

		/*
		 * Be gracious to non POSIX make programs like 'GNUmake' which
		 * may have "e -- MAKE=smake" in the MAKEFLAGS environment.
		 * The correct string would rather be "-e -- MAKE=smake".
		 */
		if (eql != NULL && (p == NULL || eql < p)) {
			/*
			 * No options at all, only cmdline macros.
			 *
			 * The content returned from getenv("MAKEFLAGS")
			 * has been reported to change on DJGPP.
			 * We need to use strdup() to keep it stable.
			 */
			MFCmdline = strdup(mf);
			if (MFCmdline == NULL)
				MFCmdline = mf;
			goto out;	/* Allow debug prints */
		}
	} else {
		hasdash = TRUE;
	}

	while (*mf) {
		switch (*mf) {

		case ' ': {
				char	*p = mf;

				while (*p == ' ')
					p++;

				if (*p != '-') {	/* May be a macro */
					if (hasdash) {
						mf = p;
						goto macs;
					}
					break;		/* Ignore blanks */
				}
				mf = p;			/* Starts with '-' */
			}
			/* FALLTHRU */

		case '-':		/* look for " -- " separator */
			if (mf[1] != '-')
				break;	/* Ignore single '-' */

			if (mf[2] != ' ') {
				char *p = nextmakemac(mf);

				errmsgno(EX_BAD,
					"Found illegal option '%s' in MAKEFLAGS.\n",
					mf);
				if (p != NULL) {
					size_t	d = p - mf;
					if (d > 50)
						d = 50;
					errmsgno(EX_BAD,
					"Skipping illegal option '%.*s'.\n",
						(int)d, mf);
					mf = p;
					break;
				} else {
					errmsgno(EX_BAD,
						"Ignoring illegal option '%s'.\n",
						mf);
				}
				goto out; /* Allow debug prints */
			}
			/*
			 * The content returned from getenv("MAKEFLAGS")
			 * has been reported to change on DJGPP.
			 * We need to use strdup() to keep it stable.
			 */
			mf += 3;
		macs:
			MFCmdline = strdup(mf);
			if (MFCmdline == NULL)
				MFCmdline = mf;
			goto out;	/* Allow debug prints */

		case 'D':		/* Display makefile */
			Dmake++;
			break;

		case 'd':		/* Debug */
			Debug++;
			break;

		case 'X':		/* XDebug */
			XDebug++;
			break;

		case 'a':
			Aflag = TRUE;	/* Do not run setup_arch() */
			break;

		case 'e':		/* Environment overrides vars */
			Eflag = TRUE;
			break;

		case 'i':		/* Ignore errors from cmds */
			Iflag = TRUE;
			break;

		case 'k':		/* Ignore target errors */
			Kflag = TRUE;
			break;

		case 'N':		/* Ignore no Source on dep. */
			NSflag = TRUE;
			break;

		case 'n':		/* Do not exec any commands */
			Nflag = TRUE;
			break;

		case 'P':		/* POSIX mode */
			posixmode = TRUE;
			break;

		case 'p':		/* Print macros/targets */
			Print = TRUE;
			break;

		case 'q':		/* Question */
			Qflag = TRUE;
			break;

		case 'r':		/* Turn off internal Rules */
			Rflag = TRUE;
			break;

		case 's':		/* Silent */
			Sflag = TRUE;
			break;

		case 'S':		/* Stop on error (opposite of -k) */
			Stopflag = TRUE;
			break;

		case 't':		/* Touch */
			Tflag = TRUE;
			break;

		case 'W':		/* Extra Warnings */
			Do_Warn++;
			break;

		case 'w':		/* No Warnings */
			No_Warn = TRUE;
			break;

		case 'Z':		/* Print includes */
			Prdep = TRUE;
			break;
		}
		mf++;
	}
out:
	/*
	 * As this is called before we call getargs(), Debug may only be true
	 * if the 'd' option is present in the MAKEFLAGS environment.
	 */
	if (Debug > 0)
		error("Read MAKEFLAGS:  '%s'\n", getenv(Makeflags));

	Mfileindex = MFsave;
}

/*
 * Parse a list of macro=value command line macros from the
 * MAKEFLAGS= environment and set up the macro in the make tree.
 * If gbuf is NULL, read_mac() is parse only.
 */
LOCAL void
read_makemacs()
{
	register char	*mf = MFCmdline;
	register char	*p;
		int	MFsave = Mfileindex;

	if (mf == NULL)
		return;

	Mfileindex = MF_IDX_ENVIRON;	/* Index 1 Environment vars */
	while (*mf) {
		p = nextmakemac(mf);
		if (p == NULL) {	/* No other macro def follows */
			if (!read_mac(mf)) {
				errmsgno(EX_BAD,
					"Bad MAKEFLAGS= environment '%s'.\n",
					getenv(Makeflags));
				*mf = '\0';
			}
			break;
		} else {		/* Need to temporarily null terminate */
			*p = '\0';
			if (!read_mac(mf)) {
				errmsgno(EX_BAD,
					"Bad MAKEFLAGS= environment '%s'.\n",
					getenv(Makeflags));
				ovstrcpy(mf, &p[1]);
			} else {
				*p = ' ';
				mf = &p[1];
			}
		}
	}
	Mfileindex = MFsave;
}

/*
 * Find next un-escaped blank (' ') which is a separator
 * for a list of macro=value items.
 */
LOCAL char *
nextmakemac(s)
	char	*s;
{
	while (*s) {
		if (*s == '\\') {	/* escaped character	*/
			if (*++s == '\0')
				return (NULL);
		} else if (*s == ' ') {	/* un-escaped space	*/
			return (s);
		}
		s++;
	}
	return (NULL);
}

/*
 * Remove the escapes that have been introduced before the name=value
 * lists are put together into the MAKEFLAGS= environment.
 * Then parse the result as a string.
 * If gbuf is NULL, read_mac() is parse only.
 */
LOCAL BOOL
read_mac(mf)
	char	*mf;
{
	char	*omf = mf;
	char	macdef[NAMEMAX*2+1];
	char	*p;

	p = macdef;
	while (*mf) {
		if (p >= &macdef[NAMEMAX*2])
			break;
		/*
		 * Only remove those escape sequences, that we created.
		 * This is "\\" and "\ ".
		 */
		if (mf[0] == '\\' && (mf[1] == '\\' || mf[1] == ' '))
			mf++;
		*p++ = *mf++;
	}
	*p = '\0';

	if (macdef[0] == '\0')			/* Ignore empty definition */
		return (TRUE);
	if (strchr(macdef, '=') == NULL) {	/* Check if it is a macro def*/
		errmsgno(EX_BAD,
			"Found illegal macro definition '%s' in MAKEFLAGS.\n",
			macdef);
		errmsgno(EX_BAD, "The raw macro definition was '%s'.\n", omf);
		return (FALSE);
	}

	if (gbuf == NULL)			/* Parse only and kill	   */
		return (TRUE);			/* unwanted content	   */

	Mflags |= F_READONLY;
	readstring(macdef, Makeflags);
	Mflags &= ~F_READONLY;
	return (TRUE);
}

/*
 * Prepare the MAKEFLAGS= environment for export.
 */
LOCAL void
setmakeflags()
{
		/*
		 * MAKEFLAGS=-	12 bytes incl '\0'
		 * 4 x 8 bytes=	32 bytes
		 * 16 flags	16 bytes
		 * '-- '	 3 bytes
		 * =====================
		 *		62 bytes
		 */
#define	MAKEENV_SIZE_STATIC	64
static	char	makeenv[MAKEENV_SIZE_STATIC];
	char	*p;
	int	i;

	p = strcatl(makeenv, Makeflags, (char *)NULL);
	*p++ = '=';
	*p++ = '-';		/* Posix make includes '-'	*/
				/* "MAKEFLAGS=-" 12 incl. '\0'	*/

	i = Dmake;		/* Display makefile */
	if (i > 8)
		i = 8;
	while (--i >= 0)
		*p++ = 'D';

	i = Debug;		/* Debug */
	if (i > 8)
		i = 8;
	while (--i >= 0)
		*p++ = 'd';

	i = Do_Warn;		/* Do_Wan - Extra Warnings */
	if (i > 8)
		i = 8;
	while (--i >= 0)
		*p++ = 'W';

	i = XDebug;		/* XDebug */
	if (i > 8)
		i = 8;
	while (--i >= 0)
		*p++ = 'X';

	if (Aflag)		/* Do not run setup_arch() */
		*p++ = 'a';
	if (Eflag)		/* Environment overrides vars */
		*p++ = 'e';
	if (Iflag)		/* Ignore errors from cmds */
		*p++ = 'i';
	if (Kflag)		/* Ignore target errors */
		*p++ = 'k';
	if (NSflag)		/* Ignore no Source on dep. */
		*p++ = 'N';
	if (Nflag)		/* Do not exec any commands */
		*p++ = 'n';
	if (posixmode)		/* POSIX mode */
		*p++ = 'P';
	if (Print)		/* Print macros/targets */
		*p++ = 'p';
	if (Qflag)		/* Question */
		*p++ = 'q';
	if (Rflag)		/* Turn off internal Rules */
		*p++ = 'r';
	if (Sflag)		/* Silent */
		*p++ = 's';
	if (Stopflag)		/* Stop on error (opposite of -k) */
		*p++ = 'S';
	if (Tflag)		/* Touch */
		*p++ = 't';
	if (No_Warn)		/* No Warnings */
		*p++ = 'w';
	if (Prdep)		/* Print includes */
		*p++ = 'Z';

	if (p - makeenv == 11)	/* Empty flags, remove '-' */
		--p;
	*p = '\0';
	define_var(Make_Flags, &makeenv[10]);	/* MAKE_FLAGS= ... */
	doexport(Make_Flags);
	setmakeenv(makeenv, p);			/* Add cmdline macs */
}

/*
 * Strip out macro defs inherited from MAKELAGS, that will be overwritten
 * by command line macro defs.
 * Return new write pointer at end of string.
 */
LOCAL char *
stripmacros(macbase, new)
	char	*macbase;
	char	*new;
{
	char	*p = strchr(new, '=');
	char	*p2;

	if (p == NULL)				/* Paranoia */
		return (macbase + strlen(macbase));

	do {
		p2 = nextmakemac(macbase);	/* Find next macro delim */

		if (strncmp(macbase, new, p - new) == 0) {
			/*
			 * Got a match, need to remove this entry.
			 */
			if (p2 == NULL) {	/* This is the only, zap out */
				*macbase = '\0';
			} else {		/* Copy rest over current */
				ovstrcpy(macbase, &p2[1]);
			}
		} else if (p2) {		/* Continue with next extry */
			macbase = &p2[1];
		}
	} while (p2);
	return (macbase + strlen(macbase));
}

/*
 * Add the actual command line macro definitions to the MAKEFLAGS= string
 * and then putenv() the result.
 */
LOCAL void
setmakeenv(envbase, envp)
	char	*envbase;
	char	*envp;
{
	register int	i;
	register int	l;
	register int	len = 0;
	register char	*p;
	register char	*macbase;

	if (Cmdlinecount == 0 && (MFCmdline == 0 || *MFCmdline == '\0')) {
		/*
		 * No command line macros and no inherited command line
		 * macros from MAKEFLAGS, so just call putenv() and return.
		 */
		put_env(envbase);
		return;
	}

	if ((envp - envbase) > 10) {	/* envbase[] is currently not empty */
		strcpy(envp, " -- ");	/* we need a separator to the flags */

		envp += 4;
		*envp = '\0';
	}
	if (MFCmdline)			/* Add one for '\0' or ' ' at end */
		len = strlen(MFCmdline) + 1;

	for (i = 0; i < Cmdlinecount; i++) {
		p = CmdLDefs[i];
		while (*p) {
			if (*p == '\\' || *p == ' ')
				len++;
			len++;
			p++;
		}
		len += 1;		/* Add one for '\0' or ' ' at end */
	}

	l = strlen(envbase) + len + 1;	/* Add one (see stripmacro comment) */
	if (l > MAKEENV_SIZE_STATIC) {
		p = malloc(l);
		strcpy(p, envbase);
		envp = p + (envp - envbase);
		envbase = p;
	}

	macbase = envp;
	if (MFCmdline) {
		for (p = MFCmdline; *p; )
			*envp++ = *p++;
		*envp++ = ' ';
	}
	*envp = '\0';

	for (i = 0; i < Cmdlinecount; i++) {
		p = CmdLDefs[i];
		envp = stripmacros(macbase, p);
		while (*p) {
			if (*p == '\\' || *p == ' ')
				*envp++ = '\\';
			*envp++ = *p++;
		}
		*envp++ = ' ';
		*envp = '\0';	/* Needed for stripmacros */
	}			/* But overshoots by one  */
	*--envp = '\0';
	put_env(envbase);
	define_var(Make_Macs, macbase);		/* MAKE_MACS= ... */
	doexport(Make_Macs);
}

#ifdef	tos
#		include "osbind.h"
#endif

/*
 * Move a target file to ObjDir.
 *
 * Returns:
 *	FALSE		Failure
 *	TRUE		OK, but no file was moved
 *	TRUE + 1	OK, but file was not found
 *	TRUE + 2	OK and file actually moved
 */
EXPORT int
move_tgt(from)
	register obj_t	*from;
{
	date_t	fromtime;
	int	code;
	char	_objname[TYPICAL_NAMEMAX];
	char	*objname = NULL;
	BOOL	ret = TRUE;

	/*
	 * Move only if:
	 *	objdir to corresponding srcdir exists
	 *	target is known in Makefile
	 */
	if ((ObjDir == NULL && from->o_level == OBJLEVEL) ||
						from->o_level < OBJLEVEL)
		return (TRUE);

	if (strchr(from->o_name, SLASH)) /* Only move from current directory */
		return (TRUE);

	fromtime = gftime(from->o_name);
	if (fromtime == 0)		/* Nothing to move found */
		return (TRUE+1);

	if (Debug > 3) error("move: from->o_level: %d\n", from->o_level);
	if ((objname = build_path(from->o_level, from->o_name, from->o_namelen,
					_objname, sizeof (_objname))) == NULL)
		return (FALSE);
	if (!Sflag || Nflag)
		gtprintf("%smove %s %s\n", posixmode?"\t":"...", from->o_name, objname);
	ret = TRUE + 2;
	if (Nflag) {
		goto out;
	}

	if ((from->o_name == objname) ||
	    (gfileid(from->o_name) == gfileid(objname))) {
		errmsgno(EX_BAD, "Will not move '%s' to itself.\n",
							from->o_name);
		ret = TRUE;
		goto out;
	}
#	ifdef	tos
	unlink(objname);
	if ((code = Frename(0, from->o_name, objname)) < 0) {
		if (code == EXDEV) {
			code = copy_file(from->o_name, objname);
			if (unlink(from->o_name) < 0)
				errmsg("Can't remove old name '%s'.\n",
								from->o_name);
		} else {
			errmsgno(-code, "Can't rename '%s' to '%s'.\n",
						from->o_name, objname);
		}
	}
	if (code < 0) {
		ret = FALSE;
		goto out;
	}
#	else
	if ((code = rename(from->o_name, objname)) < 0) {
		if (geterrno() == EXDEV) {
			if (rmdir(objname) < 0 && geterrno() == ENOTDIR)
				unlink(objname);
			code = copy_file(from->o_name, objname);
			if (unlink(from->o_name) < 0)
				errmsg("Can't remove old name '%s'.\n",
							from->o_name);
		} else {
			errmsg("Can't rename '%s' to '%s'.\n",
						from->o_name, objname);
		}
	}
	if (code < 0) {
		ret = FALSE;
		goto out;
	}
#endif	/* tos */
out:
	if (objname != NULL && objname != from->o_name && objname != _objname)
		free(objname);
	return (ret);
}

/*
 * Copy File if we cannot rename the file.
 */
#ifdef	tos
LOCAL int
copy_file(from, objname)
	char	*from;
	char	*objname;
{
	int	fin;
	int	fout;
	int	cnt = -1;

	if ((fin = open(from, O_RDONLY)) < 0)
		errmsg("Can't open '%s'.\n", from);
	else {
		if ((fout = creat(objname, 0666)) < 0)
			errmsg("Can't create '%s'.\n", objname);
		else {
			while ((cnt = read(fin, gbuf, gbufsize)) > 0) {
				if (write(fout, gbuf, cnt) != cnt) {
					errmsg("Write error on '%s'.\n",
								objname);
					close(fout);
					close(fin);
					return (-1);
				}
			}
			if (cnt < 0)
				errmsg("Read error on '%s'.\n", from);
			close(fout);
		}
		close(fin);
	}
	return (cnt);
}
#else
LOCAL int
copy_file(from, objname)
	char	*from;
	char	*objname;
{
	FILE	*fin;
	FILE	*fout;
	int	cnt = -1;

	if ((fin = fileopen(from, "rub")) == 0)
		errmsg("Can't open '%s'.\n", from);
	else {
		if ((fout = fileopen(objname, "wtcub")) == 0)
			errmsg("Can't create '%s'.\n", objname);
		else {
			file_raise(fin, FALSE);
			file_raise(fout, FALSE);
			while ((cnt = fileread(fin, gbuf, gbufsize)) > 0) {
				if (filewrite(fout, gbuf, cnt) < 0) {
					errmsg("Write error on '%s'.\n",
								objname);
					fclose(fout);
					fclose(fin);
					return (-1);
				}
			}
			if (cnt < 0)
				errmsg("Read error on '%s'.\n", from);
			fclose(fout);
		}
		fclose(fin);
	}
	return (cnt);
}
#endif

/*
 * This function behaves similar to the UNIX 'touch' command.
 */
EXPORT BOOL
touch_file(name)
	char	*name;
{
	FILE	*f;
	char	_objname[TYPICAL_NAMEMAX];
	char	*objname = _objname;
	size_t	objlen = sizeof (_objname);
	char	*np = NULL;
#ifndef	HAVE_UTIME
	char	c;
#endif

again:
	if (ObjDir == NULL)
		objname = name;
	else if (snprintf(objname, objlen, "%s%s%s", ObjDir,
				slash, filename(name)) >= objlen) {
		objlen = strlen(filename(name)) + ObjDirlen + slashlen + 1;
		objname = np = ___realloc(np, objlen, "touch path");
		goto again;
	}
#ifdef	__is_this_ok__
	if (!gftime(objname))
		snprintf(_objname, sizeof (_objname), name);
#endif
	if (!Sflag)
		gtprintf("%stouch %s\n", posixmode?"\t":"...", objname);
	if (Nflag > 0)
		return (TRUE);
	/*
	 * No touch if make was called with -n.
	 */
	if ((f = fileopen(objname, "rwcub")) != (FILE *)NULL) {
		file_raise(f, FALSE);
#ifdef	HAVE_UTIME
		utime(objname, NULL);
#else
		c = getc(f);
		fileseek(f, (off_t)0);
		putc(c, f);
#endif
		fclose(f);
		if (np)
			free(np);
		return (TRUE);
	}
	if (np)
		free(np);
	return (FALSE);
}

#	include <schily/stat.h>
#	define	STATBUF		struct stat

/*
 * Get current time.
 */
LOCAL date_t
gcurtime()
{
	struct timespec	ts;
	date_t		d;

	getnstimeofday(&ts);
	d = ts.tv_sec;
#ifdef	USE_NSECS
	d <<= NSEC_SHIFT;		/* Make space for nanoseconds */
	d |= ts.tv_nsec;
#endif
	if (Debug > 1)
		error("gcurtime() = %s (%llx)\n", prtime(d), (Llong)d);
	return (d);
}

/*
 * Get a time that is in the future (as far as possible).
 */
LOCAL date_t
gnewtime()
{
	time_t	t;
	date_t	d;

	t = TYPE_MAXVAL(time_t);

	/*
	 * "t" is now the maximum positive number for type time_t.
	 * In case of a 64 bit time_t, this is more than we can have in date_t
	 * so we need to make the number small enough to have space for nsecs.
	 */
	d = t;
#ifdef	USE_NSECS
#if SIZEOF_DATE_T > SIZEOF_TIME_T
	d <<= NSEC_SHIFT;		/* Make space for nanoseconds */
#else
	d >>= NSEC_SHIFT;		/* Shrink d to maximum time_t */
	d <<= NSEC_SHIFT;		/* Make space for nanoseconds */
#endif
	d += 999999999;			/* Add maximum nanosecond value */
#endif	/* USE_NSECS */

	while (d == NOTIME || d == BADTIME ||
			d == RECURSETIME || d == PHONYTIME) {
		d--;
	}

/*printf("i: %d, %llu, %llx, %s\n", i, d, d, prtime(-3));*/

	if (Debug > 1)
		error("gnewtime() = %s (%llx)\n", prtime(d), (Llong)d);
	return (d);
}

/*
 * Get the time of last modification for a file.
 * Cannot call it gmtime()
 */
EXPORT date_t
gftime(file)
	char	*file;
{
	STATBUF	stbuf;
	char	this_time[42];
	char	cur_time[42];
	date_t	d;

	/*
	 * XXX should we cache the file times?
	 */
/*#define	nonono__*/
#ifdef	nonono__
	obj_t	*o = objlook(file, FALSE);

	if (o != NULL && VALIDTIME(o->o_date)) {
		if (o->o_date != newtime)	/* Only if real time */
			return (o->o_date);	/* Cached file time */
	}
#endif

	stbuf.st_mtime = NOTIME;
	if (stat(file, &stbuf) < 0) {
		/*
		 * GNU libc.6 destroys st_mtime
		 */
		d = NOTIME;
	} else {
#ifdef	USE_NSECS
		long	nsecs;
#endif
		d = stbuf.st_mtime;
#ifdef	USE_NSECS
		d <<= NSEC_SHIFT;
		nsecs = stat_mnsecs(&stbuf);
#if	!defined(HAVE_UTIMENS) && !defined(HAVE_UTIMENSAT)
		/*
		 * If the current platform does not support to willfully
		 * set full nanosecond values, but only microseconds, we
		 * compare time stamps only with microsecond resolution.
		 */
		nsecs -= nsecs % 1000;
#endif
		d |= nsecs;
#endif
		/*
		 * Make sure that the time for an existing file is not
		 * in the list of special time stamps.
		 */
		while (d == NOTIME || d == BADTIME ||
		    d == RECURSETIME || d == MAKETIME || d == PHONYTIME) {
			d++;
		}
#ifdef	nonono__
		if (o == NULL) {
			o = objlook(file, TRUE);
			o->o_date = d;
		}
#endif
	}

	if (Debug > 3)
		error("gftime(%s) = %s\n", file, prtime(d));

	/*
	 * If the time stamp retrieved by stat() is more than 5 seconds
	 * ahead of our start time, we check for a possible time skew between
	 * this machine and it's fileserver.
	 */
	if (d > (curtime + (5*DATESECS))) {
		date_t	xcurtime;

		xcurtime = gcurtime();
		if (d <= (xcurtime + (5*DATESECS))) {
			/*
			 * We run for more than 5 seconds already and the
			 * file's time stamp is not more than 5 seconds ahead
			 * of the current time. Return silently.
			 */
			curtime = xcurtime;
			return (d);
		}
		strncpy(this_time, prtime(d), sizeof (this_time));
		this_time[sizeof (this_time)-1] = '\0';
		strncpy(cur_time, prtime(curtime), sizeof (cur_time));
		cur_time[sizeof (cur_time)-1] = '\0';
		errmsgno(EX_BAD,
			"WARNING: '%s' has modification time in the future (%s > %s).\n",
			file, this_time, cur_time);
	}
	return (d);
}

/*
 * Check if file is a directory.
 */
LOCAL BOOL
isdir(file)
	char	*file;
{
	STATBUF	stbuf;

	/*
	 * It is safe to assume that "." is a directory.
	 */
	if (file[0] == '.' && file[1] == '\0')
		return (TRUE);

	if (stat(file, &stbuf) < 0)
		return (TRUE);
	if ((stbuf.st_mode&S_IFMT) == S_IFDIR)
		return (TRUE);
	return (FALSE);
}

/*
 * Get a unique number for a file to prevent moving targets to themselves.
 * XXX inode number is now long !!!
 */
EXPORT Llong
gfileid(file)
	char	*file;
{
	STATBUF stbuf;
	Llong	result;

	/*
	 * Cache .OBJDIR and ".".
	 */
	if (file == ObjDir) {
		if (ObjDirfid != 0)
			return (ObjDirfid);
	} else if (file[0] == '.' && file[1] == '\0') {
		if (dotfid != 0)
			return (dotfid);
	}

	/*
	 * Setup a unique default. In case stat() will fail.
	 */
	stbuf.st_ino = (ino_t) (long) file;
	stbuf.st_dev = 0;
	if (stat(file, &stbuf) < 0) {
		/*
		 * GNU libc.6 destroys st_mtime
		 * Paranoia .... we fall back here too.
		 */
		stbuf.st_ino = (ino_t) (long) file;
		stbuf.st_dev = 0;
	}
#if	SIZEOF_LLONG > 4
	result = stbuf.st_dev;
	result <<= 32;
	result |= stbuf.st_ino;
#else
	result = stbuf.st_dev;
	result <<= 16;
	result |= stbuf.st_ino;
#endif

	if (file == ObjDir)
		ObjDirfid = result;
	else if (file[0] == '.' && file[1] == '\0')
		dotfid = result;

	if (Debug > 3)
		error("gfileid: %s %lld\n", file, result);
	return (result);
}

/*
 * Transfer UNIX time stamps into human readable names.
 * Take care of our "special timestamps".
 */
EXPORT char *
prtime(date)
	date_t	date;
{
	char	*s;
	time_t	t;
	char	nsbuf[20];
	long	nsecs;

	if (date == (date_t)0)
		return ("File does not exist");
	if (date == BADTIME)
		return ("File could not be made");
	if (date == RECURSETIME)
		return ("Recursive dependencies");
	if (date == MAKETIME)
		return ("File is currently being made");
	if (date == PHONYTIME)
		return ("File is phony");
	if (date == newtime)
		return ("Younger than any file");

#ifdef	USE_NSECS
	t = date >> NSEC_SHIFT;
#else
	t = date;
#endif
	s = ctime(&t);
	if (s == NULL)
		strcpy(s, "<time range error>");
	else
		s[strlen(s)-1] = '\0';
#ifdef	USE_NSECS
	nsecs = date & 0x3fffffff;
	snprintf(nsbuf, sizeof (nsbuf), ".%9.9ld", nsecs);
	strcat(s, nsbuf);
#endif
	return (s);
}

/*
 * Our general signal handler. Does some needed clean up and includes
 * workarounds for buggy OS like Linux.
 */
LOCAL void
handler(signo)
	int	signo;
{
	char	*name;
#if	defined(__linux__) || defined(__linux) || \
	defined(SHELL_IS_BASH) || defined(BIN_SHELL_IS_BASH)
#if	defined(HAVE_SIGNAL) && defined(HAVE_KILL) && \
			defined(SIG_IGN) && defined(SIGKILL)
	job	*jobp;
	int	i;
#endif
#endif

	signal(signo, handler);
	errmsgno(EX_BAD, "Got signal %d\n", signo);
	if (!curtarget)
		goto out;

	errmsgno(EX_BAD, "Current target is: %s precious: %d phony: %d\n",
			curtarget->o_name,
			isprecious(curtarget),
			isprecious(curtarget));

/*	while(wait(0) >= 0) */
/*		;*/
	/*
	 * Keine Bibliotheken
	 * Kein -t, -q etc.
	 */
	if (Tflag || Print || Qflag || Nflag)
		goto out;
	if (isprecious(curtarget))
		goto out;
	if (isphony(curtarget))
		goto out;

	name = curtarget->o_name;

	if (isdir(name)) {
		error("*** %s not removed.\n", name);
		goto out;
	}
	if (unlink(name) >= 0) {
		error("*** %s removed.\n", name);
	} else {
		errmsg("*** %s could not be removed.\n", name);
	}
	/*
	 * Test ob ObjDir/name existiert und neuer als vorher ist.
	 */

out:

#if	defined(__linux__) || defined(__linux) || \
	defined(SHELL_IS_BASH) || defined(BIN_SHELL_IS_BASH)
#if	defined(HAVE_SIGNAL) && defined(HAVE_KILL) && \
			defined(SIG_IGN) && defined(SIGKILL)
	/*
	 * Linux signal handling is broken. This is caused by a bug in 'bash'.
	 * Bash does jobcontrol even if called as "sh -ce 'command'".
	 * This is illegal. Only the foreground (make) process and with some
	 * bash versions the descendant 'make' processes are killed.
	 * The following code tries to kill the others too.
	 */
	signal(signo, SIG_IGN);		/* Make us immune to death ;-)	*/

	/*
	 * First shoot everyone into the foot.
	 */
	for (jobp = jobs, i = maxjobs; --i >= 0; jobp++) {
		pid_t	pid = jobp->j_pid;

		if (pid == 0)
			continue;
		kill(pid, signo);	/* Kill bash that is our child	*/
		kill(-pid, signo);	/* Kill possible bash children	*/
	}
	kill(-getpgrp(), signo);	/* Kill our process group	*/

	/*
	 * Now shoot everyone into the head.
	 */
	for (jobp = jobs, i = maxjobs; --i >= 0; jobp++) {
		pid_t	pid = jobp->j_pid;

		if (pid == 0)
			continue;
		kill(pid, SIGKILL);	/* Kill bash that is our child	*/
		kill(-pid, SIGKILL);	/* Kill possible bash children	*/
	}
	kill(-getpgrp(), SIGKILL);	/* Kill our process group	*/
#endif
#endif
	comexit(signo);
}

LOCAL void
exhandler(excode, arg)
	int	excode;
	void	*arg;
{
	/*
	 * Ausgabe wenn:
	 *
	 *	-	excode != 0 && Mlevel > 0
	 *	-	Debug > 0   && Mlevel > 0
	 */
	if ((Debug <= 1 && excode == 0) || Mlevel <= 0)
		return;

	errmsgno(EX_BAD, "Leaving  '%s'[%d] from directory '%s'\n",
			(char *)arg, Mlevel, curwdir());
	if (default_tgt != NULL)
		errmsgno(EX_BAD,
			"Default commandline target: '%s'\n", default_tgt->o_name);
	errmsgno(EX_BAD, "Doing                       exit(%d)\n", excode);
}

/*
 * Return current working directory in an allocated string.
 */
EXPORT	char *
curwdir()
{
	static	char	*wdir;
		char	wd[MAXPATHNAME + 1];

	if (wdir != NULL)
		return (wdir);

	if (getcwd(wd, MAXPATHNAME) == NULL) {
		wd[0] = '/';
		wd[1] = '\0';
	}
	wdir = malloc(strlen(wd)+1);
	if (wdir == NULL)
		comerr("Cannot malloc working dir.\n");
	strcpy(wdir, wd);
	define_var("CURDIR", wdir);
	return (wdir);
}

/*
 * Search for the defaults.smk file in the PATH of the user.
 * Assume that the file is ... bin/../share/lib/smake/defaults.smk
 */
LOCAL char *
getdefaultsfile()
{
	return (searchfileinpath("share/lib/smake/defaults.smk",
							R_OK, TRUE, NULL));
}

LOCAL int
put_env(new)
	char	*new;
{
	if (strncmp(new, "SHELL=", 6) == 0)
		return (0);		/* Never export SHELL */

	return (putenv(new));
}

LOCAL int
unset_env(name)
	char	*name;
{
	if (strcmp(name, "SHELL") == 0)
		return (0);		/* Never unexport SHELL */

	unsetenv(name);			/* OpenBSD deviates and returns void */
	return (0);
}
