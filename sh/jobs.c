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
 * This file contains modifications Copyright 2008-2011 J. Schilling
 *
 * @(#)jobs.c	1.19 11/07/19 2008-2011 J. Schilling
 */
#ifndef lint
static	UConst char sccsid[] =
	"@(#)jobs.c	1.19 11/07/19 2008-2011 J. Schilling";
#endif

/*
 * Job control for UNIX Shell
 */

#ifdef	SCHILY_BUILD
#include	<schily/termios.h>
#include	<schily/types.h>
#include	<schily/wait.h>
#include	<schily/param.h>
#include	<schily/fcntl.h>
#include	<schily/errno.h>
#else
#include	<sys/termio.h>
#include	<sys/types.h>
#include	<sys/wait.h>
#include	<sys/param.h>
#include	<fcntl.h>
#include	<errno.h>
#endif

#ifndef	WCONTINUED
#define	WCONTINUED	0
#define	WIFCONTINUED(s)	0
#endif
#ifndef	WIFCONTINUED
#define	WIFCONTINUED(s)	0
#endif
#ifndef	WNOWAIT
#define	WNOWAIT		0
#endif
#ifdef	linux
/*
 * Linux returns EINVAL if WNOWAIT is used
 */
#undef	WNOWAIT
#define	WNOWAIT		0
#endif

/*
 * one of these for each active job
 */

struct job
{
	struct job *j_nxtp;	/* next job in job ID order */
	struct job *j_curp;	/* next job in job currency order */
	struct termios j_stty;	/* termio save area when job stops */
	pid_t	j_pid;		/* job leader's process ID */
	pid_t	j_pgid;		/* job's process group ID */
	pid_t	j_tgid;		/* job's foreground process group ID */
	UInt32_t j_jid;		/* job ID */
	UInt16_t j_xval;	/* exit code, or exit or stop signal */
	UInt16_t j_flag;	/* various status flags defined below */
	char	*j_pwd;		/* job's working directory */
	char	*j_cmd;		/* cmd used to invoke this job */
};

/* defines for j_flag */

#define	J_DUMPED	0001	/* job has core dumped */
#define	J_NOTIFY	0002	/* job has changed status */
#define	J_SAVETTY	0004	/* job was stopped in foreground, and its */
				/*   termio settings were saved */
#define	J_STOPPED	0010	/* job has been stopped */
#define	J_SIGNALED	0020	/* job has received signal; j_xval has it */
#define	J_DONE		0040	/* job has finished */
#define	J_RUNNING	0100	/* job is currently running */
#define	J_FOREGND	0200	/* job was put in foreground by shell */

/* options to the printjob() function defined below */

#define	PR_CUR		00001	/* print job currency ('+', '-', or ' ') */
#define	PR_JID		00002	/* print job ID				 */
#define	PR_PGID		00004	/* print job's process group ID		 */
#define	PR_STAT		00010	/* print status obtained from wait	 */
#define	PR_CMD		00020	/* print cmd that invoked job		 */
#define	PR_AMP		00040	/* print a '&' if in the background	 */
#define	PR_PWD		00100	/* print jobs present working directory	 */

#define	PR_DFL		(PR_CUR|PR_JID|PR_STAT|PR_CMD) /* default options */
#define	PR_LONG		(PR_DFL|PR_PGID|PR_PWD)	/* long options */

static struct termios 	mystty;	 /* default termio settings		 */
static int 		eofflg,
			jobcnt,	 /* number of active jobs		 */
			jobdone, /* number of active but finished jobs	 */
			jobnote; /* jobs requiring notification		 */
static pid_t		svpgid,	 /* saved process group ID		 */
			svtgid;	 /* saved foreground process group ID	 */
static struct job 	*jobcur, /* active jobs listed in currency order */
			**nextjob,
			*thisjob,
			*joblst; /* active jobs listed in job ID order	 */

/*
 * IRIX has waitjob() in libc.
 */
#define	waitjob	sh_waitjob

static struct job *pgid2job	__PR((pid_t pgid));
static struct job *str2job	__PR((char *cmdp, char *job, int mustbejob));
static void	freejob		__PR((struct job *jp));
	void	collect_fg_job	__PR((void));
static int	statjob		__PR((struct job *jp, int exstat, int fg, int rc));
static void	collectjobs	__PR((int wnohang));
	void	freejobs	__PR((void));
static void	waitjob		__PR((struct job *jp));
static	int	settgid		__PR((pid_t new, pid_t expexted));
static void	restartjob	__PR((struct job *jp, int fg));
static void	printjob	__PR((struct job *jp, int propts));
	void	startjobs	__PR((void));
	int	endjobs		__PR((int check_if));
	void	deallocjob	__PR((void));
	void	allocjob	__PR((char *cmdp, unsigned char *cwdp, int monitor));
	void	clearjobs	__PR((void));
	void	makejob		__PR((int monitor, int fg));
	void	postjob		__PR((pid_t pid, int fg));
	void	sysjobs		__PR((int argc, char *argv[]));
	void	sysfgbg		__PR((int argc, char *argv[]));
	void	syswait		__PR((int argc, char *argv[]));
static void	sigv		__PR((char *cmdp, int sig, char *args));
	void	sysstop		__PR((int argc, char *argv[]));
	void	syskill		__PR((int argc, char *argv[]));
	void	syssusp		__PR((int argc, char *argv[]));

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
		continue;

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
			continue;
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
				if (jp != 0)
					failed((unsigned char *)cmdp, ambiguous);
				jp = njp;
			}
		}
	}

	if (mustbejob && (jp == 0 || jp->j_jid == 0))
		failed((unsigned char *)cmdp, nosuchjob);

	return (jp);
}

static void
freejob(jp)
	struct job	*jp;
{
	struct job **njp;
	struct job **cjp;

	for (njp = &joblst; *njp != jp; njp = &(*njp)->j_nxtp)
		continue;

	for (cjp = &jobcur; *cjp != jp; cjp = &(*cjp)->j_curp)
		continue;

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
	struct job *jp;
	pid_t pid;
	int exstat;

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

	while (1) {
		errno = 0;
		pid = waitpid(jp->j_pid, &exstat, 0);
		if (pid == jp->j_pid || (pid == -1 && errno == ECHILD))
			break;
	}
}

/*
 * analyze the status of a job
 */

static int
statjob(jp, exstat, fg, rc)
	struct job	*jp;
	int		exstat;
	int		fg;
	int		rc;
{
	pid_t tgid;
	int jdone = 0;

	if (WIFCONTINUED(exstat)) {
		if (jp->j_flag & J_STOPPED) {
			jp->j_flag &= ~(J_STOPPED|J_SIGNALED|J_SAVETTY);
			jp->j_flag |= J_RUNNING;
			if (!fg && jp->j_jid) {
				jp->j_flag |= J_NOTIFY;
				jobnote++;
			}
		}
	} else if (WIFSTOPPED(exstat)) {
		jp->j_xval = WSTOPSIG(exstat);
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
		if (WIFSIGNALED(exstat)) {
			jp->j_xval = WTERMSIG(exstat);
			jp->j_flag |= J_SIGNALED;
			if (WCOREDUMP(exstat))
				jp->j_flag |= J_DUMPED;
			if (!fg || jp->j_xval != SIGINT) {
				jp->j_flag |= J_NOTIFY;
				jobnote++;
			}
		} else { /* WIFEXITED */
			jp->j_xval = WEXITSTATUS(exstat);
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
		exitval = jp->j_xval;
		if (jp->j_flag & J_SIGNALED)
			exitval |= SIGFLG;
		exitset();
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
	pid_t pid;
	struct job *jp;
	int exstat, n;
	int wflags;

	if ((flags & (monitorflg|jcflg|jcoff)) == (monitorflg|jcflg))
		wflags = WUNTRACED|WCONTINUED;
	else
		wflags = 0;

	for (n = jobcnt - jobdone; n > 0; n--) {
		if ((pid = waitpid(-1, &exstat, wnohang|wflags)) <= 0)
			break;
		if ((jp = pgid2job(pid)) != NULL)
			(void) statjob(jp, exstat, 0, 0);
	}

}

void
freejobs()
{
	struct job *jp;

	collectjobs(WNOHANG);

	if (jobnote) {
		int save_fd = setb(2);
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
		for (jp = joblst; jp; jp = jp->j_nxtp) {
			if (jp->j_flag & J_DONE)
				freejob(jp);
		}
	}
}

static void
waitjob(jp)
	struct job	*jp;
{
	int exstat;
	int jdone;
	pid_t pid = jp->j_pid;
	int wflags;
	int ret = 0;
	int	err = 0;

	if ((flags & (monitorflg|jcflg|jcoff)) == (monitorflg|jcflg))
		wflags = WUNTRACED;
	else
		wflags = 0;
	do {
		errno = 0;
		ret = waitpid(pid, &exstat, wflags|WNOWAIT);
		err = errno;
		if (ret == -1 && err == ECHILD) {
			exstat = 0;
			break;
		}
	} while (ret != pid);

	jdone = statjob(jp, exstat, 1, 1);
	if (!WIFSTOPPED(exstat))		/* Avoid hang on FreeBSD */
		waitpid(pid, 0, wflags);
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
	pid_t current = tcgetpgrp(0);

	if (current != expected)
		return (current);

	if (new != current)
		tcsetpgrp(0, new);

	return (0);
}

static void
restartjob(jp, fg)
	struct job	*jp;
	int		fg;
{
	if (jp != jobcur) {
		struct job *t;
		for (t = jobcur; t->j_curp != jp; t = t->j_curp);
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
				itos(jp->j_xval);
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
			itos(jp->j_xval);
			gmsg = gettext(exited);
			sp -= strlen(gmsg) + strlen((char *)numbuf) + 2;
			prs_buff((unsigned char *)gmsg);
			prc_buff('(');
			itos(jp->j_xval);
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

	if (tcgetattr(0, &mystty) == -1 || (svtgid = tcgetpgrp(0)) == -1) {
		flags &= ~jcflg;
		return;
	}

	flags |= jcflg;

	handle(SIGTTOU, SIG_IGN);
	handle(SIGTSTP, SIG_DFL);

	if (mysid != mypgid) {
		setpgid(0, 0);
		mypgid = mypid;
		(void) settgid(mypgid, svpgid);
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
 * called by the shell to reserve a job slot for a job about to be spawned
 */

void
deallocjob()
{
	free(thisjob);
	jobcnt--;
}

void
allocjob(cmdp, cwdp, monitor)
	char		*cmdp;
	unsigned char	*cwdp;
	int		monitor;
{
	struct job *jp, **jpp;
	int jid, cmdlen, cwdlen;

	cmdlen = strlen(cmdp) + 1;
	if (cmdp[cmdlen-2] == '&') {
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
			tcsetpgrp(0, mypid);
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
			renamef(chkopen((unsigned char *)devnull, 0), 0);
	}
}

/*
 * called by the shell after job has been spawned, to fill in the
 * job slot, and wait for the job if in the foreground
 */

void
postjob(pid, fg)
pid_t pid;
int fg;
{

	int propts;

	thisjob->j_nxtp = *nextjob;
	*nextjob = thisjob;
	thisjob->j_curp = jobcur;
	jobcur = thisjob;

	if (thisjob->j_jid) {
		thisjob->j_pgid = pid;
		propts = PR_JID|PR_PGID;
	} else {
		thisjob->j_pgid = mypgid;
		propts = PR_PGID;
	}

	thisjob->j_flag = J_RUNNING;
	thisjob->j_tgid = thisjob->j_pgid;
	thisjob->j_pid = pid;
	eofflg = 0;

	if (fg) {
		thisjob->j_flag |= J_FOREGND;
		waitjob(thisjob);
	} else  {
		if (flags & ttyflg)
			printjob(thisjob, propts);
		assnum(&pcsadr, (long)pid);
	}
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
				if (bp >= brkend)
					growstak(bp);
				*bp++ = *cp++;
			}
			if (bp >= brkend)
				growstak(bp);
			*bp++ = SPACE;
		}
		endstak(bp);
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
	}

	else do
		restartjob(str2job(cmdp, *argv, 1), fg);
	while (*++argv);

}

/*
 * the builtin "wait" commands
 */

void
syswait(argc, argv)
int argc;
char *argv[];
{
	char *cmdp = *argv;
	struct job *jp;
	int exstat;
	int wflags;

	if ((flags & (monitorflg|jcflg|jcoff)) == (monitorflg|jcflg))
		wflags = WUNTRACED;
	else
		wflags = 0;

	if (argc == 1)
		collectjobs(0);
	else while (--argc) {
		if ((jp = str2job(cmdp, *++argv, 0)) == 0)
			continue;
		if (!(jp->j_flag & J_RUNNING))
			continue;
		if (waitpid(jp->j_pid, &exstat, wflags) <= 0)
			break;
		(void) statjob(jp, exstat, 0, 1);
	}
}

static void
sigv(cmdp, sig, args)
	char	*cmdp;
	int	sig;
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

	if (sig == SIGSTOP) {
		if (id == mysid || (id == mypid && mypgid == mysid)) {
			failure((unsigned char *)cmdp, loginsh);
			return;
		}
		if (id == mypgid && mypgid != svpgid) {
			(void) settgid(svtgid, mypgid);
			setpgid(0, svpgid);
			stopme++;
		}
	}

	if (pgrp)
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
				if (pgrp)
					failure((unsigned char *)cmdp, nosuchpgid);
				else
					failure((unsigned char *)cmdp, nosuchpid);
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
		sigv(cmdp, SIGSTOP, *argv);
}

void
syskill(argc, argv)
	int	argc;
	char	*argv[];
{
	char *cmdp = *argv;
	int sig = SIGTERM;

	if (argc == 1) {
		gfailure((unsigned char *)usage, killuse);
		return;
	}

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
					continue;
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
		}

		if (str2sig(&argv[1][1], &sig)) {
			failure((unsigned char *)cmdp, badsig);
			return;
		}
		argv++;
	}

	while (*++argv)
		sigv(cmdp, sig, *argv);

}

void
syssusp(argc, argv)
	int	argc;
	char	*argv[];
{
	if (argc != 1)
		failed((unsigned char *)argv[0], badopt);
	sigv(argv[0], SIGSTOP, "0");
}
