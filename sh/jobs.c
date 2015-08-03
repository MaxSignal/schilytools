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
 * Copyright 2007 Sun Microsystems, Inc.  All rights reserved.
 * Use is subject to license terms.
 */

/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

#if defined(sun)
#pragma ident	"@(#)jobs.c	1.28	07/05/14 SMI"
#endif

#include "defs.h"

/*
 * Copyright 2008-2015 J. Schilling
 *
 * @(#)jobs.c	1.60 15/07/29 2008-2015 J. Schilling
 */
#ifndef lint
static	UConst char sccsid[] =
	"@(#)jobs.c	1.60 15/07/29 2008-2015 J. Schilling";
#endif

/*
 * Job control for UNIX Shell
 */

#ifdef	SCHILY_INCLUDES
#include	<schily/termios.h>
#include	<schily/types.h>
#include	<schily/wait.h>
#include	<schily/param.h>
#include	<schily/fcntl.h>
#include	<schily/errno.h>
#include	<schily/times.h>
#include	<schily/resource.h>
#else
#include	<sys/termio.h>
#include	<sys/types.h>
#include	<sys/wait.h>
#include	<sys/param.h>
#include	<fcntl.h>
#include	<errno.h>
#include	<sys/resource.h>
#endif

#ifndef	WCONTINUED
#define	WCONTINUED	0		/* BSD from wait3() and POSIX */
#define	WIFCONTINUED(s)	0		/* Can't be there without WCONTINUED */
#endif
#ifndef	WIFCONTINUED
#define	WIFCONTINUED(s)	0		/* May be missing separately */
#endif
#ifndef	WNOWAIT
#define	WNOWAIT		0		/* SVr4 / SunOS / POSIX */
#endif
#ifndef	WEXITED
#define	WEXITED		0		/* SVr4 / SunOS / POSIX */
#endif
#ifndef	WTRAPPED
#define	WTRAPPED	0		/* SVr4 / SunOS / POSIX */
#endif
#if defined(linux) || defined(IS_MACOS_X) || defined(_IBMR2) || defined(_AIX)
/*
 * AIX, Linux and Mac OS X return EINVAL if WNOWAIT is used
 * XXX: We need to verify whether this is true as well with waitid().
 */
#undef	WNOWAIT
#define	WNOWAIT		0
#endif

#ifdef	FORCE_WAITID		/* Allow to enforce using waitid() to test */
#define	HAVE_WAITID		/* platforms where waitid() was considered */
#endif				/* unasable by "configure" .		   */
#ifdef	NO_WAITID
#undef	HAVE_WAITID
#endif
#ifndef	HAVE_WAITID		/* Need to define everything for waitid() */

/*
 * Minimal structure to emulate waitid() via waitpid().
 * In case of a waitid() emulation, we mainly get a reduced si_status range.
 */
#undef	si_code
#undef	si_pid
#undef	si_status
#undef	si_utime
#undef	si_stime

typedef struct {
	int	si_code;	/* Child status code */
	pid_t	si_pid;		/* Child pid */
	int	si_status;	/* Child exit code or signal number */
	clock_t	si_utime;
	clock_t	si_stime;
} my_siginfo_t;

#define	siginfo_t	my_siginfo_t
#define	waitid		my_waitid
#define	id_t		pid_t

#endif	/* HAVE_WAITID */

/*
 * one of these for each active job
 */
struct job
{
	struct job *j_nxtp;	/* next job in job ID order */
	struct job *j_curp;	/* next job in job currency order */
	struct termios j_stty;	/* termio save area when job stops */
#ifdef	DO_TIME
	struct timeval j_start;	/* job start time */
	struct rusage j_rustart; /* resource usage at start of job */
#endif
	pid_t	j_pid;		/* job leader's process ID */
	pid_t	j_pgid;		/* job's process group ID */
	pid_t	j_tgid;		/* job's foreground process group ID */
	UInt32_t j_jid;		/* job ID */
	Int32_t j_xval;		/* exit code, or exit or stop signal */
	Int16_t j_xcode;	/* exit or stop reason */
	UInt16_t j_flag;	/* various status flags defined below */
	char	*j_pwd;		/* job's working directory */
	char	*j_cmd;		/* cmd used to invoke this job */
};

/*
 * defines for j_flag
 */
#define	J_DUMPED	0001	/* job has core dumped */
#define	J_NOTIFY	0002	/* job has changed status */
#define	J_SAVETTY	0004	/* job was stopped in foreground, and its */
				/*   termio settings were saved */
#define	J_STOPPED	0010	/* job has been stopped */
#define	J_SIGNALED	0020	/* job has received signal; j_xval has it */
#define	J_DONE		0040	/* job has finished */
#define	J_RUNNING	0100	/* job is currently running */
#define	J_FOREGND	0200	/* job was put in foreground by shell */
#define	J_BLTIN		0400	/* job was a shell builtin */

static struct codename {
	int	c_code;		/* The si_code value */
	char	*c_name;	/* The name for si_code */
} _codename[] = {
	{ CLD_EXITED,	"EXITED" },	/* Child normal exit() */
	{ CLD_KILLED,	"KILLED" },	/* Child was killed by signal */
	{ CLD_DUMPED,	"DUMPED" },	/* Killed child dumped core */
	{ CLD_TRAPPED,	"TRAPPED" },	/* Traced child has stopped */
	{ CLD_STOPPED,	"STOPPED" },	/* Child has stopped on signal */
	{ CLD_CONTINUED, "CONTINUED" },	/* Stopped child was continued */
	{ C_NOEXEC,	"NOEXEC" },	/* No exec permissions on file */
	{ C_NOTFOUND,	"NOTFOUND" }	/* File not found */
};
#define	CODENAMESIZE	(sizeof (_codename) / sizeof (_codename[0]))


/*
 * options to the printjob() function defined below
 */
#define	PR_CUR		00001	/* print job currency ('+', '-', or ' ') */
#define	PR_JID		00002	/* print job ID				 */
#define	PR_PGID		00004	/* print job's process group ID		 */
#define	PR_STAT		00010	/* print status obtained from wait	 */
#define	PR_CMD		00020	/* print cmd that invoked job		 */
#define	PR_AMP		00040	/* print a '&' if in the background	 */
#define	PR_PWD		00100	/* print jobs present working directory	 */

#define	PR_DFL		(PR_CUR|PR_JID|PR_STAT|PR_CMD) /* default options */
#define	PR_LONG		(PR_DFL|PR_PGID|PR_PWD)	/* long options */

static struct termios	mystty;	 /* default termio settings		 */
static int		eofflg,
			jobcnt,	 /* number of active jobs		 */
			jobdone, /* number of active but finished jobs	 */
			jobnote; /* jobs requiring notification		 */
static pid_t		svpgid,	 /* saved process group ID		 */
			svtgid;	 /* saved foreground process group ID	 */
static struct job	*jobcur, /* active jobs listed in currency order */
			**nextjob,
			*thisjob,
			*joblst; /* active jobs listed in job ID order	 */

/*
 * IRIX has waitjob() in libc.
 */
#define	waitjob	sh_waitjob

static struct job *pgid2job	__PR((pid_t pgid));
static struct job *str2job	__PR((char *cmdp, char *job, int mustbejob));
	char	*code2str	__PR((int code));
static void	freejob		__PR((struct job *jp));
	void	collect_fg_job	__PR((void));
static int	statjob		__PR((struct job *jp,
					siginfo_t *si, int fg, int rc));
static void	collectjobs	__PR((int wnohang));
	void	freejobs	__PR((void));
static void	waitjob		__PR((struct job *jp));
static	int	settgid		__PR((pid_t new, pid_t expexted));
static void	restartjob	__PR((struct job *jp, int fg));
static void	printjob	__PR((struct job *jp, int propts));
	void	startjobs	__PR((void));
	int	endjobs		__PR((int check_if));
	void	deallocjob	__PR((struct job *jp));
	void	allocjob	__PR((char *cmdp,
					unsigned char *cwdp, int monitor));
	void	clearjobs	__PR((void));
	void	makejob		__PR((int monitor, int fg));
	struct job *
		postjob		__PR((pid_t pid, int fg, int blt));
	void	sysjobs		__PR((int argc, char *argv[]));
	void	sysfgbg		__PR((int argc, char *argv[]));
	void	syswait		__PR((int argc, char *argv[]));
#define	F_KILL		1
#define	F_KILLPG	2
#define	F_PGRP		3
#define	F_SUSPEND	4
static void	sigv		__PR((char *cmdp, int sig, int f, char *args));
	void	sysstop		__PR((int argc, char *argv[]));
	void	syskill		__PR((int argc, char *argv[]));
	void	syssusp		__PR((int argc, char *argv[]));
#ifdef	DO_SYSPGRP
static void	pr_pgrp		__PR((pid_t pid, pid_t pgrp, pid_t sgrp));
	void	syspgrp		__PR((int argc, char *argv[]));
#endif
	pid_t	wait_status	__PR((pid_t id,
					int *codep, int *statusp, int opts));
#ifdef	DO_TIME
	void	prtime		__PR((struct job *jp));
#endif
#ifndef	HAVE_GETRUSAGE
	int	getrusage	__PR((int who, struct rusage *r_usage));
#endif
#ifndef	HAVE_WAITID
static	int	waitid		__PR((idtype_t idtype, id_t id,
					siginfo_t *infop, int opts));
#endif

#if	!defined(HAVE_TCGETPGRP) && defined(TIOCGPGRP)
pid_t
tcgetpgrp(fd)
int fd;
{
	pid_t pgid;
	if (ioctl(fd, TIOCGPGRP, &pgid) == 0)
		return (pgid);
	return ((pid_t)-1);
}
#endif

#if	!defined(HAVE_TCSETPGRP) && defined(TIOCSPGRP)
int
tcsetpgrp(fd, pgid)
int fd;
pid_t pgid;
{
	return (ioctl(fd, TIOCSPGRP, &pgid));
}
#endif

#ifdef	PROTOTYPES
static struct job *
pgid2job(pid_t pgid)
#else
static struct job *
pgid2job(pgid)
	pid_t	pgid;
#endif
{
	struct job *jp;

	for (jp = joblst; jp != 0 && jp->j_pid != pgid; jp = jp->j_nxtp)
		/* LINTED */
		;

	return (jp);
}

static struct job *
str2job(cmdp, job, mustbejob)
	char	*cmdp;
	char	*job;
	int	mustbejob;
{
	struct job *jp, *njp;
	int i;

	if (*job != '%')
		jp = pgid2job(stoi((unsigned char *)job));
	else if (*++job == 0 || *job == '+' || *job == '%' || *job == '-') {
		jp = jobcur;
		if (*job == '-' && jp)
			jp = jp->j_curp;
	} else if (*job >= '0' && *job <= '9') {
		i = stoi((unsigned char *)job);
		for (jp = joblst; jp && jp->j_jid != i; jp = jp->j_nxtp)
			/* LINTED */
			;
	} else if (*job == '?') {
		int j;
		char *p;
		i = strlen(++job);
		jp = 0;
		for (njp = jobcur; njp; njp = njp->j_curp) {
			if (njp->j_jid == 0)
				continue;
			for (p = njp->j_cmd, j = strlen(p); j >= i; p++, j--) {
				if (strncmp(job, p, i) == 0) {
					if (jp != 0)
						failed((unsigned char *)cmdp,
						    ambiguous);
					jp = njp;
					break;
				}
			}
		}
	} else {
		i = strlen(job);
		jp = 0;
		for (njp = jobcur; njp; njp = njp->j_curp) {
			if (njp->j_jid == 0)
				continue;
			if (strncmp(job, njp->j_cmd, i) == 0) {
				if (jp != 0) {
					failed((unsigned char *)cmdp,
							ambiguous);
				}
				jp = njp;
			}
		}
	}

	if (mustbejob && (jp == 0 || jp->j_jid == 0))
		failed((unsigned char *)cmdp, nosuchjob);

	return (jp);
}

char *
code2str(code)
	int	code;
{
	int	i;

	for (i = 0; i < CODENAMESIZE; i++) {
		if (code == _codename[i].c_code)
			return (_codename[i].c_name);
	}
	return ("UNKNOWN");
}

static void
freejob(jp)
	struct job	*jp;
{
	struct job **njp;
	struct job **cjp;

	for (njp = &joblst; *njp != jp; njp = &(*njp)->j_nxtp)
		/* LINTED */
		;

	for (cjp = &jobcur; *cjp != jp; cjp = &(*cjp)->j_curp)
		/* LINTED */
		;

	*njp = jp->j_nxtp;
	*cjp = jp->j_curp;
	free(jp);
	jobcnt--;
	jobdone--;
}

/*
 * Collect the foreground job.
 * Used in the case where the subshell wants
 * to exit, but needs to wait until the fg job
 * is done.
 */
void
collect_fg_job()
{
	struct job	*jp;
	int		err;
	siginfo_t	si;

	for (jp = joblst; jp; jp = jp->j_nxtp)
		if (jp->j_flag & J_FOREGND)
			break;

	if (!jp)
		/* no foreground job */
		return;

	/*
	 * Wait on fg job until wait succeeds
	 * or it fails due to no waitable children.
	 */

	/* CONSTCOND */
	while (1) {
		errno = 0;
		si.si_pid = 0;
		err = waitid(P_PID, jp->j_pid, &si, (WEXITED|WTRAPPED));
		if (si.si_pid == jp->j_pid || (err == -1 && errno == ECHILD))
			break;
	}
}

/*
 * analyze the status of a job
 */
static int
statjob(jp, si, fg, rc)
	struct job	*jp;
	siginfo_t	*si;
	int		fg;
	int		rc;
{
	int	code = si->si_code;
	pid_t tgid;
	int jdone = 0;

	jp->j_xcode = code;
	if (code == CLD_CONTINUED) {
		if (jp->j_flag & J_STOPPED) {
			jp->j_flag &= ~(J_STOPPED|J_SIGNALED|J_SAVETTY);
			jp->j_flag |= J_RUNNING;
			if (!fg && jp->j_jid) {
				jp->j_flag |= J_NOTIFY;
				jobnote++;
			}
		}
	} else if (code == CLD_STOPPED || code == CLD_TRAPPED) {
		jp->j_xval = si->si_status;		/* Stopsig */
		jp->j_flag &= ~J_RUNNING;
		jp->j_flag |= (J_SIGNALED|J_STOPPED);
		jp->j_pgid = getpgid(jp->j_pid);
		jp->j_tgid = jp->j_pgid;
		if (fg) {
			if ((tgid = settgid(mypgid, jp->j_pgid)) != 0)
				jp->j_tgid = tgid;
			else {
				jp->j_flag |= J_SAVETTY;
				tcgetattr(0, &jp->j_stty);
				(void) tcsetattr(0, TCSANOW, &mystty);
			}
		}
		if (jp->j_jid) {
			jp->j_flag |= J_NOTIFY;
			jobnote++;
		}
	} else {
		jp->j_flag &= ~J_RUNNING;
		jp->j_flag |= J_DONE;
		jdone++;
		jobdone++;
		if (code == CLD_KILLED || code == CLD_DUMPED) {
			jp->j_xval = si->si_status;	/* Termsig */
			jp->j_flag |= J_SIGNALED;
			if (code == CLD_DUMPED)
				jp->j_flag |= J_DUMPED;
			if (!fg || jp->j_xval != SIGINT) {
				jp->j_flag |= J_NOTIFY;
				jobnote++;
			}
		} else { /* CLD_EXITED */
			jp->j_xval = si->si_status;	/* Exit status */
			jp->j_flag &= ~J_SIGNALED;
			if (!fg && jp->j_jid) {
				jp->j_flag |= J_NOTIFY;
				jobnote++;
			}
		}
		if (fg) {
			if (!settgid(mypgid, jp->j_pgid) ||
			    !settgid(mypgid, getpgid(jp->j_pid)))
				tcgetattr(0, &mystty);
		}
	}
	if (rc) {
		/*
		 * First check whether we have a prefilled exit code
		 * from a previous vfork()d child that matches this child.
		 */
		if (ex.ex_code < C_NOEXEC || ex.ex_pid != si->si_pid) {
			ex.ex_status = exitval = jp->j_xval;
			ex.ex_code = jp->j_xcode;
			ex.ex_pid = si->si_pid;
		}
#ifdef	SIGCHLD
		ex.ex_signo = SIGCHLD;
#else
		ex.ex_signo = 1000;	/* Need to distinct this from builtin */
#endif
		if ((flags2 & fullexitcodeflg) == 0)
			exitval &= 0xFF; /* As dumb as with historic wait */
		if (jp->j_flag & J_SIGNALED)
			exitval |= SIGFLG;
		else if (ex.ex_status != 0 && exitval == 0)
			exitval = SIGFLG; /* Use special value 128 */
		exitset();		/* Set retval from exitval for $? */
	}
	if (jdone && !(jp->j_flag & J_NOTIFY))
		freejob(jp);
	return (jdone);
}

/*
 * collect the status of jobs that have recently exited or stopped -
 * if wnohang == WNOHANG, wait until error, or all jobs are accounted for;
 *
 * called after each command is executed, with wnohang == 0, and as part
 * of "wait" builtin with wnohang == WNOHANG
 *
 * We do not need to call chktrap here if waitpid(2) is called with
 * wnohang == 0, because that only happens from syswait() which is called
 * from builtin() where chktrap() is already called.
 */
static void
collectjobs(wnohang)
int wnohang;
{
	pid_t		pid;
	struct job	*jp;
	int		n;
	siginfo_t	si;
	int		wflags;

	if ((flags & (monitorflg|jcflg|jcoff)) == (monitorflg|jcflg))
		wflags = WUNTRACED|WCONTINUED;
	else
		wflags = 0;
	wflags |= (WEXITED|WTRAPPED);	/* Needed for waitid() */

	for (n = jobcnt - jobdone; n > 0; n--) {
		if (waitid(P_ALL, 0, &si, wnohang|wflags) < 0)
			break;
		pid = si.si_pid;
		if (pid == 0)
			break;
		if ((jp = pgid2job(pid)) != NULL)
			(void) statjob(jp, &si, 0, 0);
	}
}

void
freejobs()
{
	struct job *jp;

	collectjobs(WNOHANG);

	if (jobnote) {
		int save_fd = setb(STDERR_FILENO);
		for (jp = joblst; jp; jp = jp->j_nxtp) {
			if (jp->j_flag & J_NOTIFY) {
				if (jp->j_jid)
					printjob(jp, PR_DFL);
				else if (jp->j_flag & J_FOREGND)
					printjob(jp, PR_STAT);
				else
					printjob(jp, PR_STAT|PR_PGID);
			}
		}
		(void) setb(save_fd);
	}

	if (jobdone) {
		struct job *sjp;

		for (jp = joblst; jp; jp = sjp) {
			sjp = jp->j_nxtp;
			if (jp->j_flag & J_DONE)
				freejob(jp);
		}
	}
}

static void
waitjob(jp)
	struct job	*jp;
{
	siginfo_t	si;
	int		jdone;
	pid_t		pid = jp->j_pid;
	int		wflags;
	int		ret = 0;
	int		err = 0;

#ifdef	DO_TIME
	getrusage(RUSAGE_CHILDREN, &jp->j_rustart);
#endif

	if ((flags & (monitorflg|jcflg|jcoff)) == (monitorflg|jcflg))
		wflags = WUNTRACED;
	else
		wflags = 0;
	wflags |= (WEXITED|WTRAPPED);	/* Needed for waitid() */
	do {
		errno = 0;
		ret = waitid(P_PID, pid, &si, wflags|WNOWAIT);
		err = errno;
		if (ret == -1 && err == ECHILD) { /* No children */
			si.si_status = 0;
			si.si_code = 0;
			si.si_pid = 0;
			break;
		}
		/*
		 * si.si_pid == 0: no status is available for pid
		 */
	} while (si.si_pid != pid);

#if	WNOWAIT != 0
	/*
	 * Avoid hang on FreeBSD, so wait/reap here only for died children.
	 */
	if (si.si_code != CLD_STOPPED && si.si_code != CLD_TRAPPED) {
		siginfo_t	si2;

		waitid(P_PID, pid, &si2, wflags);
		si.si_utime = si2.si_utime;
		si.si_stime = si2.si_stime;
	}
#endif
	jdone = statjob(jp, &si, 1, 1);	/* Sets exitval, see below */

#ifdef	DO_TIME
	if (flags2 & timeflg)
		prtime(jp);
#endif

	if (jdone && exitval && (flags & errflg))
		exitsh(exitval);
	flags |= eflag;
}

/*
 * modify the foreground process group to *new* only if the
 * current foreground process group is equal to *expected*
 */
static int
settgid(new, expected)
pid_t new, expected;
{
	pid_t current = tcgetpgrp(STDIN_FILENO);

	if (current != expected)
		return (current);

	if (new != current)
		tcsetpgrp(STDIN_FILENO, new);

	return (0);
}

static void
restartjob(jp, fg)
	struct job	*jp;
	int		fg;
{
	if (jp != jobcur) {
		struct job *t;
		for (t = jobcur; t->j_curp != jp; t = t->j_curp)
			/* LINTED */
			;
		t->j_curp = jp->j_curp;
		jp->j_curp = jobcur;
		jobcur = jp;
	}
	if (fg) {
		if (jp->j_flag & J_SAVETTY) {
			jp->j_stty.c_lflag &= ~TOSTOP;
			jp->j_stty.c_lflag |= (mystty.c_lflag&TOSTOP);
			jp->j_stty.c_cc[VSUSP] = mystty.c_cc[VSUSP];
#ifdef	VDSUSP
			jp->j_stty.c_cc[VDSUSP] = mystty.c_cc[VDSUSP];
#endif
			(void) tcsetattr(0, TCSADRAIN, &jp->j_stty);
		}
		(void) settgid(jp->j_tgid, mypgid);
	}
	(void) kill(-(jp->j_pgid), SIGCONT);
	if (jp->j_tgid != jp->j_pgid)
		(void) kill(-(jp->j_tgid), SIGCONT);
	jp->j_flag &= ~(J_STOPPED|J_SIGNALED|J_SAVETTY);
	jp->j_flag |= J_RUNNING;
	if (fg)  {
		jp->j_flag |= J_FOREGND;
		printjob(jp, PR_JID|PR_CMD);
		waitjob(jp);
	} else {
		jp->j_flag &= ~J_FOREGND;
		printjob(jp, PR_JID|PR_CMD|PR_AMP);
	}
}

static void
printjob(jp, propts)
	struct job	*jp;
	int		propts;
{
	int sp = 0;

	if (jp->j_flag & J_NOTIFY) {
		jobnote--;
		jp->j_flag &= ~J_NOTIFY;
	}

	if (propts & PR_JID) {
		prc_buff('[');
		prn_buff(jp->j_jid);
		prc_buff(']');
		sp = 1;
	}

	if (propts & PR_CUR) {
		while (sp-- > 0)
			prc_buff(SPACE);
		sp = 1;
		if (jobcur == jp)
			prc_buff('+');
		else if (jobcur != 0 && jobcur->j_curp == jp)
			prc_buff('-');
		else
			sp++;
	}

	if (propts & PR_PGID) {
		while (sp-- > 0)
			prc_buff(SPACE);
		prn_buff(jp->j_pid);
		sp = 1;
	}

	if (propts & PR_STAT) {
		const char	*gmsg;
		while (sp-- > 0)
			prc_buff(SPACE);
		sp = 28;
		if (jp->j_flag & J_SIGNALED) {
			const char	*sigstr;

			if ((sigstr = strsignal(jp->j_xval)) != NULL) {
				sp -= strlen(sigstr);
				prs_buff((unsigned char *)sigstr);
			} else {
				sitos(jp->j_xval);
				gmsg = gettext(signalnum);
				sp -= strlen((char *)numbuf) + strlen(gmsg);
				prs_buff((unsigned char *)gmsg);
				prs_buff(numbuf);
			}
			if (jp->j_flag & J_DUMPED) {
				gmsg = gettext(coredump);
				sp -= strlen(gmsg);
				prs_buff((unsigned char *)gmsg);
			}
		} else if (jp->j_flag & J_DONE) {
			sitos(jp->j_xval);
			gmsg = gettext(exited);
			sp -= strlen(gmsg) + strlen((char *)numbuf) + 2;
			prs_buff((unsigned char *)gmsg);
			prc_buff('(');
			sitos(jp->j_xval);
			prs_buff(numbuf);
			prc_buff(')');
		} else {
			gmsg = gettext(running);
			sp -= strlen(gmsg);
			prs_buff((unsigned char *)gmsg);
		}
		if (sp < 1)
			sp = 1;
	}

	if (propts & PR_CMD) {
		while (sp-- > 0)
			prc_buff(SPACE);
		prs_buff((unsigned char *)jp->j_cmd);
		sp = 1;
	}

	if (propts & PR_AMP) {
		while (sp-- > 0)
			prc_buff(SPACE);
		prc_buff('&');
		sp = 1;
	}

	if (propts & PR_PWD) {
		while (sp-- > 0)
			prc_buff(SPACE);
		prs_buff((unsigned char *)"(wd: ");
		prs_buff((unsigned char *)jp->j_pwd);
		prc_buff(')');
	}

	prc_buff(NL);
	flushb();
}

/*
 * called to initialize job control for each new input file to the shell,
 * and after the "exec" builtin
 */
void
startjobs()
{
	svpgid = mypgid;

	if (tcgetattr(0, &mystty) == -1 ||
	    (svtgid = tcgetpgrp(STDIN_FILENO)) == -1) {
		flags &= ~jcflg;
		return;
	}

	flags |= jcflg;

	handle(SIGTTOU, SIG_IGN);
	handle(SIGTSTP, SIG_DFL);

	if (mysid != mypgid) {
		setpgid(0, 0);		/* Make me a process group leader */
		mypgid = mypid;		/* and remember my new pgid	  */
		(void) settgid(mypgid, svpgid);	/* and set up my tty pgrp */
	}
}

int
endjobs(check_if)
int check_if;
{
	if ((flags & (jcoff|jcflg)) != jcflg)
		return (1);

	if (check_if && jobcnt && eofflg++ == 0) {
		struct job *jp;
		if (check_if & JOB_STOPPED) {
			for (jp = joblst; jp; jp = jp->j_nxtp) {
				if (jp->j_jid && (jp->j_flag & J_STOPPED)) {
					prs(_gettext(jobsstopped));
					prc(NL);
					return (0);
				}
			}
		}
		if (check_if & JOB_RUNNING) {
			for (jp = joblst; jp; jp = jp->j_nxtp) {
				if (jp->j_jid && (jp->j_flag & J_RUNNING)) {
					prs(_gettext(jobsrunning));
					prc(NL);
					return (0);
				}
			}
		}
	}

	if (svpgid != mypgid) {
		(void) settgid(svtgid, mypgid);
		setpgid(0, svpgid);
	}

	return (1);
}

/*
 * Called by the shell to destroy a job slot from allocjob() if spawn failed.
 * We need to check whether we really need to have the jp parameter for
 * timing of builtin commands.
 */
void
deallocjob(jp)
	struct job	*jp;
{
	if (jp == NULL)
		jp = thisjob;
	free(jp);
	jobcnt--;
}

/*
 * Called by the shell to reserve a job slot for a job about to be spawned.
 * Resulting job slot is in "thisjob".
 */
void
allocjob(cmdp, cwdp, monitor)
	char		*cmdp;
	unsigned char	*cwdp;
	int		monitor;
{
	struct job *jp, **jpp;
	int jid, cmdlen, cwdlen;

	cmdlen = strlen(cmdp) + 1;
	if (cmdlen > 1 && cmdp[cmdlen-2] == '&') {
		cmdp[cmdlen-3] = 0;
		cmdlen -= 2;
	}
	cwdlen = strlen((char *)cwdp) + 1;
	jp = (struct job *) alloc(sizeof (struct job) + cmdlen + cwdlen);
	if (jp == 0)
		error(nostack);
	jobcnt++;
	jp->j_cmd = ((char *)jp) + sizeof (struct job);
	strcpy(jp->j_cmd, cmdp);
	jp->j_pwd = jp->j_cmd + cmdlen;
	strcpy(jp->j_pwd, (char *)cwdp);
	jp->j_nxtp = jp->j_curp = NULL;

	jpp = &joblst;

	if (monitor) {
		for (; *jpp; jpp = &(*jpp)->j_nxtp)
			if ((*jpp)->j_jid != 0)
				break;
		for (jid = 1; *jpp; jpp = &(*jpp)->j_nxtp, jid++)
			if ((*jpp)->j_jid != jid)
				break;
	} else
		jid = 0;

	jp->j_jid = jid;
	nextjob = jpp;
	thisjob = jp;
}

void
clearjobs()
{
	struct job *jp, *sjp;

	for (jp = joblst; jp; jp = sjp) {
		sjp = jp->j_nxtp;
		free(jp);
	}
	joblst = NULL;
	jobcnt = 0;
	jobnote = 0;
	jobdone = 0;
}

void
makejob(monitor, fg)
	int	monitor;
	int	fg;
{
	if (monitor) {
		mypgid = mypid;
		setpgid(0, 0);
		if (fg)
			tcsetpgrp(STDIN_FILENO, mypid);
		handle(SIGTTOU, SIG_DFL);
		handle(SIGTSTP, SIG_DFL);
	} else if (!fg) {
#ifdef NICE
		nice(NICE);
#endif
		handle(SIGTTIN, SIG_IGN);
		handle(SIGINT,  SIG_IGN);
		handle(SIGQUIT, SIG_IGN);
		if (!ioset)
			renamef(chkopen((unsigned char *)devnull, O_RDONLY),
					STDIN_FILENO);
	}
}

/*
 * called by the shell after job has been spawned, to fill in the
 * job slot, and wait for the job if in the foreground
 */
struct job *
postjob(pid, fg, blt)
	pid_t	pid;	/* The pid of the new job			*/
	int	fg;	/* Whether this is a foreground job		*/
	int	blt;	/* Whether this is a temp slot for a builtin	*/
{
	int propts;

	if (!blt) {	/* Do not connect slots for builtin commands	*/
		thisjob->j_nxtp = *nextjob;
		*nextjob = thisjob;
		thisjob->j_curp = jobcur;
		jobcur = thisjob;
	}

	if (thisjob->j_jid) {
		thisjob->j_pgid = pid;
		propts = PR_JID|PR_PGID;
	} else {
		thisjob->j_pgid = mypgid;
		propts = PR_PGID;
	}

#ifdef	DO_TIME
	gettimeofday(&thisjob->j_start, NULL);
#endif
	thisjob->j_flag = J_RUNNING;
	thisjob->j_tgid = thisjob->j_pgid;
	thisjob->j_pid = pid;
	eofflg = 0;

	if (fg) {
		thisjob->j_flag |= J_FOREGND;
		if (blt) {
			thisjob->j_flag |= J_BLTIN;
#ifdef	DO_TIME
			getrusage(RUSAGE_SELF, &thisjob->j_rustart);
#endif
		} else {
			waitjob(thisjob);
		}
	} else  {
		if (flags & ttyflg)
			printjob(thisjob, propts);
		assnum(&pcsadr, (long)pid);
	}
	return (thisjob);
}

/*
 * the builtin "jobs" command
 */
void
sysjobs(argc, argv)
int argc;
char *argv[];
{
	char *cmdp = *argv;
	struct job *jp;
	int propts, c;
	extern int opterr;
	int savoptind = optind;
	int loptind = -1;
	int savopterr = opterr;
	int savsp = _sp;
	char *savoptarg = optarg;
	optind = 1;
	opterr = 0;
	_sp = 1;
	propts = 0;

	if ((flags & jcflg) == 0)
		failed((unsigned char *)cmdp, nojc);

	while ((c = getopt(argc, argv, "lpx")) != -1) {
		if (propts) {
			gfailure((unsigned char *)usage, jobsuse);
			goto err;
		}
		switch (c) {
			case 'x':
				propts = -1;
				break;
			case 'p':
				propts = PR_PGID;
				break;
			case 'l':
				propts = PR_LONG;
				break;
			case '?':
				gfailure((unsigned char *)usage, jobsuse);
				goto err;
		}
	}

	loptind = optind;
err:
	optind = savoptind;
	optarg = savoptarg;
	opterr = savopterr;
	_sp = savsp;
	if (loptind == -1)
		return;

	if (propts == -1) {
		unsigned char *bp;
		char *cp;
		unsigned char *savebp;
		for (savebp = bp = locstak(); loptind < argc; loptind++) {
			cp = argv[loptind];
			if (*cp == '%') {
				jp = str2job(cmdp, cp, 1);
				itos(jp->j_pid);
				cp = (char *)numbuf;
			}
			while (*cp) {
				GROWSTAK(bp);
				*bp++ = *cp++;
			}
			GROWSTAK(bp);
			*bp++ = SPACE;
		}
		savebp = endstak(bp);
		execexp(savebp, (Intptr_t)0);
		return;
	}

	collectjobs(WNOHANG);

	if (propts == 0)
		propts = PR_DFL;

	if (loptind == argc) {
		for (jp = joblst; jp; jp = jp->j_nxtp) {
			if (jp->j_jid)
				printjob(jp, propts);
		}
	} else do
		printjob(str2job(cmdp, argv[loptind++], 1), propts);
	while (loptind < argc);
}

/*
 * the builtin "fg" and "bg" commands
 */
/* ARGSUSED */
void
sysfgbg(argc, argv)
	int	argc;
	char	*argv[];
{
	char *cmdp = *argv;
	int fg;

	if ((flags & jcflg) == 0)
		failed((unsigned char *)cmdp, nojc);

	fg = eq("fg", cmdp);

	if (*++argv == 0) {
		struct job *jp;
		for (jp = jobcur; ; jp = jp->j_curp) {
			if (jp == 0)
				failed((unsigned char *)cmdp, nocurjob);
			if (jp->j_jid)
				break;
		}
		restartjob(jp, fg);
	} else {
		do {
			restartjob(str2job(cmdp, *argv, 1), fg);
		} while (*++argv);
	}
}

/*
 * the builtin "wait" commands
 */
void
syswait(argc, argv)
int argc;
char *argv[];
{
	char		*cmdp = *argv;
	struct job	*jp;
	int		wflags;
	siginfo_t	si;

	if ((flags & (monitorflg|jcflg|jcoff)) == (monitorflg|jcflg))
		wflags = WUNTRACED;
	else
		wflags = 0;
	wflags |= (WEXITED|WTRAPPED);	/* Needed for waitid() */

	if (argc == 1)
		collectjobs(0);
	else while (--argc) {
		if ((jp = str2job(cmdp, *++argv, 0)) == 0)
			continue;
		if (!(jp->j_flag & J_RUNNING))
			continue;
		if (waitid(P_PID, jp->j_pid, &si, wflags) < 0)
			break;
		(void) statjob(jp, &si, 0, 1);
	}
}

static void
sigv(cmdp, sig, f, args)
	char	*cmdp;
	int	sig;
	int	f;
	char	*args;
{
	int pgrp = 0;
	int stopme = 0;
	pid_t id;

	if (*args == '%') {
		struct job *jp;
		jp = str2job(cmdp, args, 1);
		id = jp->j_pgid;
		pgrp++;
	} else {
		if (*args == '-') {
			pgrp++;
			args++;
		}
		id = 0;
		do {
			if (*args < '0' || *args > '9') {
				failure((unsigned char *)cmdp, badid);
				return;
			}
			id = (id * 10) + (*args - '0');
		} while (*++args);
		if (id == 0) {
			id = mypgid;
			pgrp++;
		}
	}

#ifdef	DO_SYSPGRP
	if (f == F_PGRP) {
		pid_t	pgid = getpgid(id);

		if (pgid == (pid_t)-1) {
			flushb();
			failure((unsigned char *)cmdp, "cannot get pgid");
		} else {
			pid_t	sgrp = (pid_t)-1;

#ifdef	HAVE_GETSID
			sgrp = getsid(id);
#endif
			pr_pgrp(id, pgid, sgrp);
		}
		return;
	}
#endif

	if (sig == SIGSTOP) {
		/*
		 * If the id equals our session group id, this is the id
		 * of the session group leader and thus the login shell.
		 *
		 * If the id equals our process id and our process group id
		 * equals our session group id, we are the login shell.
		 */
		if (id == mysid || (id == mypid && mypgid == mysid)) {
			failure((unsigned char *)cmdp, loginsh);
			return;
		}

		/*
		 * If the id equals our process group id and our process group
		 * id differs from our saved process group id, we are not the
		 * login shell, but need to restore the previous process
		 * group id first.
		 */
		if (id == mypgid && mypgid != svpgid) {
			(void) settgid(svtgid, mypgid);
			setpgid(0, svpgid);
			stopme++;
			if (f == F_SUSPEND)	/* Called by the suspend cmd */
				id = svpgid;	/* Stop our caller as well */
		}
	}

	if (pgrp || f == F_KILLPG)
		id = -id;

	if (kill(id, sig) < 0) {

		switch (errno) {
			case EPERM:
				failure((unsigned char *)cmdp, eacces);
				break;

			case EINVAL:
				failure((unsigned char *)cmdp, badsig);
				break;

			default:
				if (pgrp) {
					failure((unsigned char *)cmdp,
							nosuchpgid);
				} else {
					failure((unsigned char *)cmdp,
							nosuchpid);
				}
				break;
		}

	} else if (sig == SIGTERM && pgrp)
		(void) kill(id, SIGCONT);

	if (stopme) {
		setpgid(0, mypgid);
		(void) settgid(mypgid, svpgid);
	}
}

void
sysstop(argc, argv)
	int	argc;
	char	*argv[];
{
	char *cmdp = *argv;
	if (argc <= 1) {
		gfailure((unsigned char *)usage, stopuse);
		return;
	}
	while (*++argv)
		sigv(cmdp, SIGSTOP, F_KILL, *argv);
}

void
syskill(argc, argv)
	int	argc;
	char	*argv[];
{
	char *cmdp = *argv;
	int sig = SIGTERM;
	int	pg;

	if (argc == 1) {
		gfailure((unsigned char *)usage, killuse);
		return;
	}
	pg = eq("killpg", cmdp);

	if (argv[1][0] == '-') {

		if (argc == 2) {

			int i;
			int cnt = 0;
			char sep = 0;
			char buf[12];

			if (!eq(argv[1], "-l")) {
				gfailure((unsigned char *)usage, killuse);
				return;
			}

			for (i = 1; i < MAXTRAP; i++) {
				if (sig2str(i, buf) < 0)
					strcpy(buf, "bad sig");	/* continue; */
				if (sep)
					prc_buff(sep);
				prs_buff((unsigned char *)buf);
				if ((flags & ttyflg) && (++cnt % 10))
					sep = TAB;
				else
					sep = NL;
			}
			prc_buff(NL);
			return;
#ifdef	DO_KILL_L_SIG
		} else if (eq(argv[1], "-l")) {
			char buf[12];

			++argv;
			while (*++argv) {
				sig = stoi((unsigned char *)*argv) & 0x7F;
				if (sig2str(sig, buf) < 0) {
					failure((unsigned char *)*argv, badsig);
					return;
				}
				prs_buff((unsigned char *)buf);
				prc_buff(NL);
			}
			return;
#endif
		}

		if (str2sig(&argv[1][1], &sig)) {
			failure((unsigned char *)cmdp, badsig);
			return;
		}
		argv++;
	}

	while (*++argv)
		sigv(cmdp, sig, pg ? F_KILLPG : F_KILL, *argv);
}

void
syssusp(argc, argv)
	int	argc;
	char	*argv[];
{
	if (argc != 1)
		failed((unsigned char *)argv[0], badopt);
	sigv(argv[0], SIGSTOP, F_SUSPEND, "0");
}

#ifdef	DO_SYSPGRP
static void
pr_pgrp(pid, pgrp, sgrp)
	pid_t	pid;
	pid_t	pgrp;
	pid_t	sgrp;
{
	prs_buff(UC "pid: ");
	prs_buff(&numbuf[ltos((long)pid)]);
	prs_buff(UC " processgroup: ");
	prs_buff(&numbuf[ltos((long)pgrp)]);
	if (sgrp != (pid_t)-1) {
		prs_buff(UC " sessiongroup: ");
		prs_buff(&numbuf[ltos((long)sgrp)]);
	}
	prc_buff(NL);
}

void
syspgrp(argc, argv)
	int	argc;
	char	*argv[];
{
	char *cmdp = *argv;

	if (argc == 1) {
		pid_t	pgrp = tcgetpgrp(STDIN_FILENO);
		pid_t	sgrp = (pid_t)-1;

#ifdef	HAVE_GETSID
		sgrp = getsid(0);
#endif
		prs_buff(UC "ttyprocessgroup: ");
		prs_buff(&numbuf[ltos((long)pgrp)]);
		prc_buff(NL);
		pr_pgrp(mypid, mypgid, sgrp);
		return;
	}
#if	!defined(HAVE_GETPGID) && !defined(HAVE_BSD_GETPGRP)
	failure((unsigned char *)cmdp, unimplemented);
#else
	while (*++argv)
		sigv(cmdp, 0, F_PGRP, *argv);
#endif
}
#endif

void
hupforegnd()
{
	struct job *jp;
	sigset_t set, oset;

	/*
	 * add SIGCHLD to mask
	 */
	sigemptyset(&set);
	sigaddset(&set, SIGCHLD);
	sigprocmask(SIG_BLOCK, &set, &oset);
	for (jp = joblst; jp != NULL; jp = jp->j_nxtp) {
		if (jp->j_flag & J_FOREGND) {
			(void) kill(jp->j_pid, SIGHUP);
			break;
		}
	}
	sigprocmask(SIG_SETMASK, &oset, 0);
}

/*
 * Simple interface to waitid() that avoids the need to use our internal
 * siginfo_t emulation and our internal idtype_t emulation in case the
 * platform does not offer waitid().
 * It still allows to return more than the low 8 bits from exit().
 */
pid_t
wait_status(pid, codep, statusp, opts)
	pid_t		pid;
	int		*codep;
	int		*statusp;
	int		opts;
{
	siginfo_t	si;
	pid_t		ret;
	idtype_t	idtype;
	pid_t		id;

	if (pid > 0) {
		idtype = P_PID;
		id = pid;
	} else if (pid < -1) {
		idtype = P_PGID;
		id = -pid;
	} else if (pid == -1) {
		idtype = P_ALL;
		id = 0;
	} else {
		idtype = P_PGID;
		id = getpgid(0);
	}
	ret = waitid(idtype, id, &si, opts);
	if (ret == (pid_t)-1)
		return ((pid_t)-1);
	if (codep)
		*codep = si.si_code;
	if (statusp)
		*statusp = si.si_status;
	return (si.si_pid);
}

#ifdef	DO_TIME
void
prtime(jp)
	struct job	*jp;
{
	struct timeval	stop;
	struct rusage	rustop;
	UIntmax_t	cpu;
	UIntmax_t	per;
	int		save_fd = setb(STDERR_FILENO);

	gettimeofday(&stop, NULL);
	if (jp->j_flag & J_BLTIN)
		getrusage(RUSAGE_SELF, &rustop);
	else
		getrusage(RUSAGE_CHILDREN, &rustop);

	timersub(&stop, &jp->j_start);
	timersub(&rustop.ru_utime, &jp->j_rustart.ru_utime);
	timersub(&rustop.ru_stime, &jp->j_rustart.ru_stime);

	cpu =  rustop.ru_utime.tv_sec*1000 + rustop.ru_utime.tv_usec/1000;
	cpu += rustop.ru_stime.tv_sec*1000 + rustop.ru_stime.tv_usec/1000;
	per = stop.tv_sec*1000 + stop.tv_usec/1000;
	if (per < 1)
		per = 1;
	per = 100 * cpu / per;

	prc_buff('r');
	prtv(&stop, FALSE);
	prc_buff(SPACE);
	prc_buff('u');
	prtv(&rustop.ru_utime, FALSE);
	prc_buff(SPACE);
	prc_buff('s');
	prtv(&rustop.ru_stime, FALSE);
	prc_buff(SPACE);
	prull_buff(per);
	prc_buff('%');
	prc_buff(NL);
	flushb();

	(void) setb(save_fd);
}
#endif	/* DO_TIME */

#ifndef	HAVE_GETRUSAGE
int
getrusage(who, r_usage)
	int		who;
	struct rusage	*r_usage;
{
	int		ret = -1;
#ifdef	HAVE_TIMES
	struct tms	tms;

	times(&tms);
#endif
	memset(r_usage, 0, sizeof (*r_usage));
#ifdef	HAVE_TIMES
	if (who == RUSAGE_SELF) {
		clock2tv(tms.tms_utime, &r_usage->ru_utime);
		clock2tv(tms.tms_stime, &r_usage->ru_stime);
	} else if (who == RUSAGE_CHILDREN) {
		clock2tv(tms.tms_cutime, &r_usage->ru_utime);
		clock2tv(tms.tms_cstime, &r_usage->ru_stime);
	}
#endif
	return (ret);
}
#endif	/* HAVE_GETRUSAGE */

#ifndef	HAVE_WAITID
static int
waitid(idtype, id, infop, opts)
	idtype_t	idtype;
	id_t		id;
	siginfo_t	*infop;		/* Must be != NULL */
	int		opts;
{
	int		exstat;
	pid_t		pid;
#ifdef	__needed__
	struct tms	tms;
#endif

	opts &= ~(WEXITED|WTRAPPED);	/* waitpid() doesn't understand them */

	if (idtype == P_PID)
		pid = id;
	else if (idtype == P_PGID)
		pid = -id;
	else if (idtype == P_ALL)
		pid = -1;
	else
		pid = 0;

	infop->si_utime = 0;
	infop->si_stime = 0;
#ifdef	__needed__
	if (WNOWAIT == 0 || (opts & WNOWAIT) == 0) {
		times(&tms);
	}
#endif
	pid = waitpid(pid, &exstat, opts);
	infop->si_pid = pid;
	infop->si_code = 0;
	infop->si_status = 0;

	if (pid == (pid_t)-1)
		return (-1);

#ifdef	__needed__
	if (WNOWAIT == 0 || (opts & WNOWAIT) == 0) {
		infop->si_utime = tms.tms_cutime;
		infop->si_stime = tms.tms_cstime;
		times(&tms);
		infop->si_utime = tms.tms_cutime - infop->si_utime;
		infop->si_stime = tms.tms_cstime - infop->si_stime;
	}
#endif

	if (WIFEXITED(exstat)) {
		infop->si_code = CLD_EXITED;
		infop->si_status = WEXITSTATUS(exstat);
	} else if (WIFSIGNALED(exstat)) {
		if (WCOREDUMP(exstat))
			infop->si_code = CLD_DUMPED;
		else
			infop->si_code = CLD_KILLED;
		infop->si_status = WTERMSIG(exstat);
	} else if (WIFSTOPPED(exstat)) {
#ifdef	SIGTRAP
		if (WSTOPSIG(exstat) == SIGTRAP)
			infop->si_code = CLD_TRAPPED;
		else
#endif
			infop->si_code = CLD_STOPPED;
		infop->si_status = WSTOPSIG(exstat);
	} else if (WIFCONTINUED(exstat)) {
		infop->si_code = CLD_CONTINUED;
		infop->si_status = 0;
	}

	return (0);
}
#endif
