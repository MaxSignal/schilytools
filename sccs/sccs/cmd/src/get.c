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
/* Copyright (c) 1988 AT&T */
/* All Rights Reserved */
/*
 * Copyright 2003 Sun Microsystems, Inc. All rights reserved.
 * Use is subject to license terms.
 */
/*
 * This file contains modifications Copyright 2006-2008 J. Schilling
 *
 * @(#)get.c	1.13 08/06/14 J. Schilling
 */
#if defined(sun) || defined(__GNUC__)

#ident "@(#)get.c 1.13 08/06/14 J. Schilling"
#endif
/*
 * @(#)get.c 1.59 06/12/12
 */

#ident	"@(#)get.c"
#ident	"@(#)sccs:cmd/get.c"

#include	<defines.h>
#include	<version.h>
#include	<had.h>
#include	<i18n.h>
#include	<sys/utsname.h>
#include	<ccstypes.h>
#include	<limits.h>
#include	<sysexits.h>

#define	DATELEN	12

# ifdef __STDC__
#define INCPATH		INS_BASE "/ccs/include"	/* place to find binaries */
#else
/*
 * XXX With a K&R compiler, you need to edit the following string in case
 * XXX you like to change the install path.
 */
#define INCPATH		"/usr/ccs/include"	/* place to find binaries */
#endif



/*
 * Get is now much more careful than before about checking
 * for write errors when creating the g- l- p- and z-files.
 * However, it remains deliberately unconcerned about failures
 * in writing statistical information (e.g., number of lines
 * retrieved).
 */

struct stat Statbuf;
char Null[1];
char SccsError[MAXERRORLEN];

struct sid sid;
int	Did_id;

static struct packet gpkt;
static unsigned	Ser;
static int	num_files;
static int	num_ID_lines;
static int	cnt_ID_lines;
static int	expand_IDs;
static char	*list_expand_IDs;
static char    *Whatstr = NULL;
static char	Pfilename[FILESIZE];
static char	*ilist, *elist, *lfile;
static time_t	cutoff = (time_t)0X7FFFFFFFL;	/* max positive long */
static char	Gfile[PATH_MAX];
static char	gfile[PATH_MAX];
static char	*Type;
static struct utsname un;
static char *uuname;

/* Beginning of modifications made for CMF system. */
#define CMRLIMIT 128
static char	cmr[CMRLIMIT];
static int	cmri = 0;
/* End of insertion */

	void    clean_up __PR((void));
	void	escdodelt __PR((struct packet *pkt));
	void	fredck	__PR((struct packet *pkt));
	void	enter	__PR((struct packet *pkt, int ch, int n, struct sid *sidp));

	int	main __PR((int argc, char **argv));
static void	get __PR((char *file));
static void	gen_lfile __PR((struct packet *pkt));
static void	idsetup __PR((struct packet *pkt));
static void	makgdate __PR((char *old, char *new));
static void	makgdatel __PR((char *old, char *new));
static char *	idsubst __PR((struct packet *pkt, char *line));
static char *	_trans __PR((char *tp, char *str, char *rest));
static int	readcopy __PR((char *name, char *tp));
static void	prfx __PR((struct packet *pkt));
static void	wrtpfile __PR((struct packet *pkt, char *inc, char *exc));
static int	cmrinsert __PR((void));

int
main(argc,argv)
int argc;
register char *argv[];
{
	register int i;
	register char *p;
	int  c;
	extern int Fcnt;
	int current_optind;
	int no_arg;

	/*
	 * Set locale for all categories.
	 */
	setlocale(LC_ALL, NOGETTEXT(""));
	/* 
	 * Set directory to search for general l10n SCCS messages.
	 */
#ifdef	PROTOTYPES
	bindtextdomain(NOGETTEXT("SUNW_SPRO_SCCS"),
	   NOGETTEXT(INS_BASE "/ccs/lib/locale/"));
#else
	bindtextdomain(NOGETTEXT("SUNW_SPRO_SCCS"),
	   NOGETTEXT("/usr/ccs/lib/locale/"));
#endif
	textdomain(NOGETTEXT("SUNW_SPRO_SCCS"));

	Fflags = FTLEXIT | FTLMSG | FTLCLN;
	current_optind = 1;
	optind = 1;
	opterr = 0;
	no_arg = 0;
	i = 1;
	/*CONSTCOND*/
	while (1) {
			if (current_optind < optind) {
			   current_optind = optind;
			   argv[i] = 0;
			   if (optind > i+1) {
			      if ((argv[i+1][0] != '-') && (no_arg == 0)) {
			         argv[i+1] = NULL;
			      } else {
				 optind = i+1;
			         current_optind = optind;
			      }
			   }
			}
			no_arg = 0;
			i = current_optind;
		        c = getopt(argc, argv, "-r:c:ebi:x:kl:Lpsmngta:G:w:zqdV(version)");
				/* this takes care of options given after
				** file names.
				*/
			if (c == EOF) {
			   if (optind < argc) {
			      /* if it's due to -- then break; */
			      if (argv[i][0] == '-' &&
				  argv[i][1] == '-') {
			         argv[i] = 0;
			         break;
			      }
			      optind++;
			      current_optind = optind;
			      continue;
			   } else {
			      break;
			   }
			}
			p = optarg;
			switch (c) {
    			case CMFFLAG:
				/* Concatenate the rest of this argument with 
				 * the existing CMR list. */
				if (p) {
				   while (*p) {
				      if (cmri == CMRLIMIT)
					 fatal(gettext("CMR list is too long."));
					 cmr[cmri++] = *p++;
				   }
				}
				cmr[cmri] = '\0';
				break;
			case 'a':
				if (*p == 0) continue;
				Ser = patoi(p);
				break;
			case 'r':
				if (argv[i][2] == '\0') {
				   if (*(omit_sid(p)) != '\0') {
				      no_arg = 1;
				      continue;
				   }
				}
				chksid(sid_ab(p, &sid), &sid);
				if ((sid.s_rel < MINR) || (sid.s_rel > MAXR)) {
				   fatal(gettext("r out of range (ge22)"));
				}
				break;
			case 'w':
				if (p) Whatstr = p;
				break;
			case 'c':
				if (*p == 0) continue;
				if (parse_date(p,&cutoff))
				   fatal(gettext("bad date/time (cm5)"));
				break;
			case 'L':
				/* treat this as if -lp was given */
				lfile = NOGETTEXT("stdout");
				c = 'l';
				break;
			case 'l':
				if (argv[i][2] != '\0') {
				   if ((*p == 'p') && (strlen(p) == 1)) {
				      p = NOGETTEXT("stdout");
				   }
				   lfile = p;
				} else {
				   no_arg = 1;
				   lfile = NULL;
				}	
				break;
			case 'i':
				if (*p == 0) continue;
				ilist = p;
				break;
			case 'x':
				if (*p == 0) continue;
				elist = p;
				break;
			case 'G':
				{
				char *cp;

				if (*p == 0) continue;
				copy(p, gfile);
 				cp = auxf(p, 'G');
				copy(cp, Gfile);
				break;
				}
			case 'b':
			case 'g':
			case 'e':
			case 'p':
			case 'k':
			case 'm':
			case 'n':
			case 's':
			case 't':
			case 'd':
				if (p) {
				   sprintf(SccsError,
				     gettext("value after %c arg (cm8)"), 
				     c);
				   fatal(SccsError);
				}
				break;
                        case 'q': /* enable NSE mode */
				if (p) {
                                   if (*p) {
                                      nsedelim = p;
				   }
                                } else {
                                   nsedelim = (char *) 0;
                                }
                                break;

			case 'V':		/* version */
				printf("get %s-SCCS version %s (%s-%s-%s)\n",
					PROVIDER,
					VERSION,
					HOST_CPU, HOST_VENDOR, HOST_OS);
				exit(EX_OK);

			default:
			   fatal(gettext("Usage: get [-begkmLpst] [-l[p]] [-asequence] [-cdate-time] [-Gg-file] [-isid-list] [-rsid] [-xsid-list] file ..."));
			
			}

			/* The following is necessary in case the */
			/* user types some localized character,  */
			/* which will exceed the limits of the */
			/* array "had", defined in ../hdr/had.h . */
			/* This guard is also necessary in case the */
			/* user types a capital ascii character, in */
			/* which case the had[] array reference will */
			/* be out of bounds.  */
			if (!((c - 'a') < 0 || (c - 'a') > 25)) {
			   if (had[c - 'a']++)
			      fatal(gettext("key letter twice (cm2)"));
			}
	}
	for (i=1; i<argc; i++) {
		if (argv[i]) {
		       num_files++;
		}
	}
	if(num_files == 0)
		fatal(gettext("missing file arg (cm3)"));
	if (HADE && HADM)
		fatal(gettext("e not allowed with m (ge3)"));
	if (HADE)
		HADK = 1;
	if (HADE && !logname())
		fatal(gettext("User ID not in password file (cm9)"));
	setsig();
	Fflags &= ~FTLEXIT;
	Fflags |= FTLJMP;
	for (i=1; i<argc; i++)
		if ((p=argv[i]) != NULL)
			do_file(p, get, 1);

	return (Fcnt ? 1 : 0);
}

extern char *Sflags[];

static void
get(file)
char *file;
{
	register char *p;
	register unsigned ser;
	extern char had_dir, had_standinp;
	struct stats stats;
	char	str[32];
#ifdef	PROTOTYPES
	char template[] = NOGETTEXT("/get.XXXXXX");
#else
	char *template = NOGETTEXT("/get.XXXXXX");
#endif
	char buf1[PATH_MAX];
	uid_t holduid;
	gid_t holdgid;
	static int first = 1;

	if (setjmp(Fjmp))
		return;
	if (HADE) {
		/*
		call `sinit' to check if file is an SCCS file
		but don't open the SCCS file.
		If it is, then create lock file.
		pass both the PID and machine name to lockit
		*/
		sinit(&gpkt,file,0);
		uname(&un);
		uuname = un.nodename;
		if (lockit(auxf(file,'z'),10, getpid(),uuname))
			fatal(gettext("cannot create lock file (cm4)"));
	}
	/*
	Open SCCS file and initialize packet
	*/
	sinit(&gpkt,file,1);
	gpkt.p_ixuser = (HADI | HADX);
	gpkt.p_reqsid.s_rel = sid.s_rel;
	gpkt.p_reqsid.s_lev = sid.s_lev;
	gpkt.p_reqsid.s_br = sid.s_br;
	gpkt.p_reqsid.s_seq = sid.s_seq;
	gpkt.p_verbose = (HADS) ? 0 : 1;
	gpkt.p_stdout  = (HADP||lfile) ? stderr : stdout;
	gpkt.p_cutoff = cutoff;
	gpkt.p_lfile = lfile;
	if (Gfile[0] == 0 || !first) {
		copy(auxf(gpkt.p_file,'g'),gfile);
		copy(auxf(gpkt.p_file,'A'),Gfile);
	}
	strcpy(buf1, dname(Gfile));
	strcat(buf1, template);
	Gfile[0] = '\0';		/* File not yet created */
	first = 0;

	if (gpkt.p_verbose && (num_files > 1 || had_dir || had_standinp))
		fprintf(gpkt.p_stdout,"\n%s:\n",gpkt.p_file);
	if (dodelt(&gpkt,&stats,(struct sid *) 0,0) == 0)
		fmterr(&gpkt);
	finduser(&gpkt);
	doflags(&gpkt);
	if ((p = Sflags[SCANFLAG - 'a']) == NULL) {
		num_ID_lines = 0;
	} else {
		num_ID_lines = atoi(p);
	}
	expand_IDs = 0;
	if ((list_expand_IDs = Sflags[EXPANDFLAG - 'a']) != NULL) {
		if (*list_expand_IDs) {
			/*
			 * The old Sun based code did break with get -k in case
			 * that someone did previously call admin -fy...
			 * Make sure that this now works correctly again.
			 */
			if (!HADK)		/* JS fix get -k */
				expand_IDs = 1;
		}
	}
	if (HADE) {
		expand_IDs = 0;
	}
	if (!HADK) {
		expand_IDs = 1;
	}
	if (!HADA) {
		ser = getser(&gpkt);
	} else {
		if ((ser = Ser) > maxser(&gpkt))
			fatal(gettext("serial number too large (ge19)"));
		gpkt.p_gotsid = gpkt.p_idel[ser].i_sid;
		if (HADR && sid.s_rel != gpkt.p_gotsid.s_rel) {
			zero((char *) &gpkt.p_reqsid, sizeof(gpkt.p_reqsid));
			gpkt.p_reqsid.s_rel = sid.s_rel;
		} else {
			gpkt.p_reqsid = gpkt.p_gotsid;
		}
	}
	doie(&gpkt,ilist,elist,(char *) 0);
	setup(&gpkt,(int) ser);
	if ((Type = Sflags[TYPEFLAG - 'a']) == NULL)
		Type = Null;
	if (!(HADP || HADG)) {
		if (exists(gfile) && (S_IWRITE & Statbuf.st_mode)) {
			sprintf(SccsError,gettext("writable `%s' exists (ge4)"),
				gfile);
			fatal(SccsError);
		}
	}
	if (gpkt.p_verbose) {
		sid_ba(&gpkt.p_gotsid,str);
		fprintf(gpkt.p_stdout,"%s\n",str);
	}
	if (HADE) {
		if (HADC || gpkt.p_reqsid.s_rel == 0)
			gpkt.p_reqsid = gpkt.p_gotsid;
		newsid(&gpkt,Sflags[BRCHFLAG - 'a'] && HADB);
		permiss(&gpkt);
		wrtpfile(&gpkt,ilist,elist);
	}
	if (!HADG || HADL) {
		if (gpkt.p_stdout) {
			fflush(gpkt.p_stdout);
			fflush (stderr);
		}
		holduid=geteuid();
		holdgid=getegid();
		setuid(getuid());
		setgid(getgid());
		if (HADL)
			gen_lfile(&gpkt);
		if (HADG)
			goto unlock;
		flushto(&gpkt,EUSERTXT,1);
		idsetup(&gpkt);
		gpkt.p_chkeof = 1;
		Did_id = 0;
		/*
		call `xcreate' which deletes the old `g-file' and
		creates a new one except if the `p' keyletter is set in
		which case any old `g-file' is not disturbed.
		The mod of the file depends on whether or not the `k'
		keyletter has been set.
		*/
		if (gpkt.p_gout == 0) {
			if (HADP) {
				gpkt.p_gout = stdout;
			} else {
#ifdef	HAVE_MKSTEMP
				close(mkstemp(buf1));	/* safer than mktemp */
#else
				mktemp(buf1);
#endif
				copy(buf1, Gfile);
				/*
				 * It may be better to use fdopen() and chmod()
				 * instead of xfcreat() in order to avoind an
				 * unlink()/create() chain.
				 */
				if (exists(gpkt.p_file) && (S_IEXEC & Statbuf.st_mode)) {
					gpkt.p_gout = xfcreat(Gfile,HADK ? 
						((mode_t)0755) : ((mode_t)0555));
				} else {
					gpkt.p_gout = xfcreat(Gfile,HADK ? 
						((mode_t)0644) : ((mode_t)0444));
				}
			}
		}
		if (Sflags[ENCODEFLAG - 'a'] &&
		    (strcmp(Sflags[ENCODEFLAG - 'a'],"1") == 0)) {
			while (readmod(&gpkt)) {
				decode(gpkt.p_line,gpkt.p_gout);
			}
		} else {
			while (readmod(&gpkt)) {
				prfx(&gpkt);
				p = idsubst(&gpkt,gpkt.p_line);
				if (fputs(p,gpkt.p_gout)==EOF)
					xmsg(gfile, NOGETTEXT("get"));
			}
		}
		if (gpkt.p_gout) {
			if (fflush(gpkt.p_gout) == EOF)
				xmsg(gfile, NOGETTEXT("get"));
			fflush (stderr);
		}
		if (gpkt.p_gout && gpkt.p_gout != stdout) {
			/*
			 * Force g-file to disk and verify
			 * that it actually got there.
			 */
			if (fsync(fileno(gpkt.p_gout)) < 0)
				xmsg(gfile, NOGETTEXT("get"));
			if (fclose(gpkt.p_gout) == EOF)
				xmsg(gfile, NOGETTEXT("get"));
			gpkt.p_gout = NULL;
		}
		if (gpkt.p_verbose) {
#ifdef XPG4		
		   fprintf(gpkt.p_stdout, NOGETTEXT("%d lines\n"), gpkt.p_glnno);
#else
		   if (HADD == 0)
		      fprintf(gpkt.p_stdout,gettext("%d lines\n"),gpkt.p_glnno);
#endif
		}		
		if (!Did_id && !HADK && !HADQ) {
		   if (Sflags[IDFLAG - 'a']) {
		      if (!(*Sflags[IDFLAG - 'a'])) {
		         fatal(gettext("no id keywords (cm6)"));
		      } else {
			 fatal(gettext("invalid id keywords (cm10)"));
		      }
		   } else {
		      if (gpkt.p_verbose) {
			 fprintf(stderr, gettext("No id keywords (cm7)\n"));
		      }
		   }
		}
		if (Gfile[0] != '\0' && exists(Gfile) ) {
			rename(Gfile, gfile);
		}
		setuid(holduid);
		setgid(holdgid);
	}
	if (gpkt.p_iop) {
		fclose(gpkt.p_iop);
		gpkt.p_iop = NULL;
	}
unlock:
	if (HADE) {
		copy(auxf(gpkt.p_file,'p'),Pfilename);
		rename(auxf(gpkt.p_file,'q'),Pfilename);
		uname(&un);
		uuname = un.nodename;
		unlockit(auxf(gpkt.p_file,'z'), getpid(),uuname);
	}
	ffreeall();
}

void
enter(pkt,ch,n,sidp)
struct packet *pkt;
char ch;
int n;
struct sid *sidp;
{
	char str[32];
	register struct apply *ap;

	sid_ba(sidp,str);
	if (pkt->p_verbose)
		fprintf(pkt->p_stdout,"%s\n",str);
	ap = &pkt->p_apply[n];
	switch (ap->a_code) {

	case SX_EMPTY:
		if (ch == INCLUDE)
			condset(ap,APPLY,INCLUSER);
		else
			condset(ap,NOAPPLY,EXCLUSER);
		break;
	case APPLY:
		sid_ba(sidp,str);
		sprintf(SccsError, gettext("%s already included (ge9)"), str);
		fatal(SccsError);
		break;
	case NOAPPLY:
		sid_ba(sidp,str);
		sprintf(SccsError, gettext("%s already excluded (ge10)"), str);
		fatal(SccsError);
		break;
	default:
		fatal(gettext("internal error in get/enter() (ge11)"));
		break;
	}
}

static void
gen_lfile(pkt)
register struct packet *pkt;
{
	char *n;
	int reason;
	char str[32];
	char line[BUFSIZ];
	struct deltab dt;
	FILE *in;
	FILE *out;
	char *outname = NOGETTEXT("stdout");

#define	OUTPUTC(c) \
	if (putc((c), out) == EOF) \
		xmsg(outname, NOGETTEXT("gen_lfile"));

	in = xfopen(pkt->p_file, O_RDONLY|O_BINARY);
	if (pkt->p_lfile) {
		out = stdout;
	} else {
		outname = auxf(pkt->p_file, 'l');
		out = xfcreat(outname,(mode_t)0444);
	}
	fgets(line,sizeof(line),in);
	while (fgets(line,sizeof(line),in) != NULL &&
	       line[0] == CTLCHAR && line[1] == STATS) {
		fgets(line,sizeof(line),in);
		del_ab(line,&dt,pkt);
		if (dt.d_type == 'D') {
			reason = pkt->p_apply[dt.d_serial].a_reason;
			if (pkt->p_apply[dt.d_serial].a_code == APPLY) {
				OUTPUTC(' ');
				OUTPUTC(' ');
			} else {
				OUTPUTC('*');
				if (reason & IGNR) {
					OUTPUTC(' ');
				} else {
					OUTPUTC('*');
				}
			}
			switch (reason & (INCL | EXCL | CUTOFF)) {
	
			case INCL:
				OUTPUTC('I');
				break;
			case EXCL:
				OUTPUTC('X');
				break;
			case CUTOFF:
				OUTPUTC('C');
				break;
			default:
				OUTPUTC(' ');
				break;
			}
			OUTPUTC(' ');
			sid_ba(&dt.d_sid,str);
			if (fprintf(out, "%s\t", str) == EOF)
				xmsg(outname, NOGETTEXT("gen_lfile"));
			if (dt.d_datetime > Y2038)
				date_bal(&dt.d_datetime,str);	/* 4 digit year */
			else
				date_ba(&dt.d_datetime,str);	/* 2 digit year */
			if (fprintf(out, "%s %s\n", str, dt.d_pgmr) == EOF)
				xmsg(outname, NOGETTEXT("gen_lfile"));
		}
		while ((n = fgets(line,sizeof(line),in)) != NULL) {
			if (line[0] != CTLCHAR) {
				break;
			} else {
				switch (line[1]) {

				case EDELTAB:
					break;
				default:
					continue;
				case MRNUM:
				case COMMENTS:
					if (dt.d_type == 'D') {
					   if (fprintf(out,"\t%s",&line[3]) == EOF)
					      xmsg(outname, 
					         NOGETTEXT("gen_lfile"));
					}
					continue;
				}
				break;
			}
		}
		if (n == NULL || line[0] != CTLCHAR)
			break;
		OUTPUTC('\n');
	}
	fclose(in);
	if (out != stdout) {
		if (fsync(fileno(out)) < 0)
			xmsg(outname, NOGETTEXT("gen_lfile"));
		if (fclose(out) == EOF)
			xmsg(outname, NOGETTEXT("gen_lfile"));
	}

#undef	OUTPUTC
}

static char	Curdatel[20];
static char	Curdate[18];
static char	*Curtime;
static char	Gdate[DATELEN];
static char	Gdatel[DATELEN];
static char	Chgdate[18];
static char	Chgdatel[20];
static char	*Chgtime;
static char	Gchgdate[DATELEN];
static char	Gchgdatel[DATELEN];
static char	Sid[32];
static char	Mod[FILESIZE];
static char	Olddir[BUFSIZ];
static char	Pname[BUFSIZ];
static char	Dir[BUFSIZ];
static char	*Qsect;

static void
idsetup(pkt)
register struct packet *pkt;
{
	extern time_t Timenow;
	register int n;
	register char *p;

	date_ba(&Timenow,Curdate);
	date_bal(&Timenow,Curdatel);
	Curdatel[10] = 0;
	Curtime = &Curdate[9];
	Curdate[8] = 0;
	makgdate(Curdate,Gdate);
	makgdatel(Curdatel,Gdatel);
	for (n = maxser(pkt); n; n--)
		if (pkt->p_apply[n].a_code == APPLY)
			break;
	if (n) {
		date_ba(&pkt->p_idel[n].i_datetime, Chgdate);
		date_bal(&pkt->p_idel[n].i_datetime, Chgdatel);
	} else {
#ifdef XPG4
		FILE	*xf;

		if (exists(gfile))
			unlink(gfile);
		xf = xfcreat(gfile, HADK ? ((mode_t)0644) : ((mode_t)0444));
		if (xf)
			fclose(xf);
#endif
		fatal(gettext("No sid prior to cutoff date-time (ge23)"));
	}
	Chgdatel[10] = 0;
	Chgtime = &Chgdate[9];
	Chgdate[8] = 0;
	makgdate(Chgdate,Gchgdate);
	makgdatel(Chgdatel,Gchgdatel);
	sid_ba(&pkt->p_gotsid,Sid);
	if ((p = Sflags[MODFLAG - 'a']) != NULL)
		copy(p,Mod);
	else
		copy(auxf(gpkt.p_file,'g'), Mod);
	if ((Qsect = Sflags[QSECTFLAG - 'a']) == NULL)
		Qsect = Null;
}

#ifdef	HAVE_STRFTIME
static void 
makgdate(old,new)
register char *old, *new;
{
	struct tm	tm;
	sscanf(old, "%d/%d/%d", &(tm.tm_year), &(tm.tm_mon), &(tm.tm_mday));
	tm.tm_mon--;
	strftime (new, (size_t) DATELEN, NOGETTEXT("%D"), &tm);
}

static void 
makgdatel(old,new)
register char *old, *new;
{
	struct tm	tm;
	sscanf(old, "%d/%d/%d", &(tm.tm_year), &(tm.tm_mon), &(tm.tm_mday));
	if (tm.tm_year > 1900)
		tm.tm_year -= 1900;
	tm.tm_mon--;
	strftime (new, (size_t) DATELEN, NOGETTEXT("%m/%d/%Y"), &tm);
}
	

#else	/* HAVE_STRFTIME not defined */

static void
makgdate(old,new)
register char *old, *new;
{
#define	NULL_PAD_DATE		/* Make it compatible to strftime()/man page */
#ifndef	NULL_PAD_DATE
	if ((*new = old[3]) != '0')
		new++;
#else
	*new++ = old[3];	
#endif
	*new++ = old[4];
	*new++ = '/';
#ifndef	NULL_PAD_DATE
	if ((*new = old[6]) != '0')
		new++;
#else
	*new++ = old[6];
#endif
	*new++ = old[7];
	*new++ = '/';
	*new++ = old[0];
	*new++ = old[1];
	*new = 0;
}

static void
makgdatel(old,new)
register char *old, *new;
{
	char	*p;
	int	off;

	for (p = old; *p >= '0' && *p <= '9'; p++)
		;
	off = p - old - 2;
	if (off < 0)
		off = 0;
	
#ifndef	NULL_PAD_DATE
	if ((*new = old[3+off]) != '0')
		new++;
#else
	*new++ = old[3+off];
#endif
	*new++ = old[4+off];
	*new++ = '/';
#ifndef	NULL_PAD_DATE
	if ((*new = old[6+off]) != '0')
		new++;
#else
	*new++ = old[6+off];
#endif
	*new++ = old[7+off];
	*new++ = '/';
	*new++ = old[0];
	*new++ = old[1];
	if (off > 0)
		*new++ = old[2];
	if (off > 1)
		*new++ = old[3];
	*new = 0;
}
#endif	/* HAVE_STRFTIME */


static char Zkeywd[5] = "@(#)";
static char *tline = NULL;
static size_t tline_size = 0;

#define trans(a, b)	_trans(a, b, lp)

static char *
idsubst(pkt,line)
register struct packet *pkt;
char line[];
{
	static char *hold = NULL;
	static size_t hold_size = 0;
	size_t new_hold_size;
	static char str[32];
	register char *lp, *tp;
	int recursive = 0;
	char *expand_ID;
		
	cnt_ID_lines++;
	if (HADK) {
		if (!expand_IDs)
			return(line);
	} else {
		if (!any('%', line))
			return(line);
	}
	if (cnt_ID_lines > num_ID_lines) {
		if (!expand_IDs) {
			return(line);
		}
		if (num_ID_lines) {
			expand_IDs = 0;
			return(line);
		}
	}
	tp = tline;
	for (lp=line; *lp != 0; lp++) {
		if ((!Did_id) && (Sflags['i'-'a']) &&
		    (!(strncmp(Sflags['i'-'a'],lp,strlen(Sflags['i'-'a'])))))
				++Did_id;
		if (lp[0] == '%' && lp[1] != 0 && lp[2] == '%') {
			expand_ID = ++lp;
			if (!HADK && expand_IDs && (list_expand_IDs != NULL)) {
				if (!any(*expand_ID, list_expand_IDs)) {
					str[3] = '\0';
					strncpy(str, lp - 1, 3);
					tp = trans(tp, str);
					lp++;
					continue;
				}
			}
			switch (*expand_ID) {

			case 'M':
				tp = trans(tp,Mod);
				break;
			case 'Q':
				tp = trans(tp,Qsect);
				break;
			case 'R':
				sprintf(str,"%d",pkt->p_gotsid.s_rel);
				tp = trans(tp,str);
				break;
			case 'L':
				sprintf(str,"%d",pkt->p_gotsid.s_lev);
				tp = trans(tp,str);
				break;
			case 'B':
				sprintf(str,"%d",pkt->p_gotsid.s_br);
				tp = trans(tp,str);
				break;
			case 'S':
				sprintf(str,"%d",pkt->p_gotsid.s_seq);
				tp = trans(tp,str);
				break;
			case 'D':
				tp = trans(tp,Curdate);
				break;
			case 'd':
				tp = trans(tp,Curdatel);
				break;
			case 'H':
				tp = trans(tp,Gdate);
				break;
			case 'h':
				tp = trans(tp,Gdatel);
				break;
			case 'T':
				tp = trans(tp,Curtime);
				break;
			case 'E':
				tp = trans(tp,Chgdate);
				break;
			case 'e':
				tp = trans(tp,Chgdatel);
				break;
			case 'G':
				tp = trans(tp,Gchgdate);
				break;
			case 'g':
				tp = trans(tp,Gchgdatel);
				break;
			case 'U':
				tp = trans(tp,Chgtime);
				break;
			case 'Z':
				tp = trans(tp,Zkeywd);
				break;
			case 'Y':
				tp = trans(tp,Type);
				break;
			/*FALLTHRU*/
			case 'W':
				if ((Whatstr != NULL) && (recursive == 0)) {
					recursive = 1;
					lp += 2;
					new_hold_size = strlen(lp) + strlen(Whatstr) + 1;
					if (new_hold_size > hold_size) {
						if (hold)
							free(hold);
						hold_size = new_hold_size;
						hold = (char *)malloc(hold_size);
						if (hold == NULL) {
							fatal(gettext("OUT OF SPACE (ut9)"));
						}
					}
					strcpy(hold,Whatstr);
					strcat(hold,lp);
					lp = hold;
					lp--;
					continue;
				}
				tp = trans(tp,Zkeywd);
				tp = trans(tp,Mod);
				tp = trans(tp,"\t");
			case 'I':
				tp = trans(tp,Sid);
				break;
			case 'P':
				copy(pkt->p_file,Dir);
				dname(Dir);
				if (getcwd(Olddir,sizeof(Olddir)) == NULL)
				   fatal(gettext("getcwd() failed (ge20)"));
				if (chdir(Dir) != 0)
				   fatal(gettext("cannot change directory (ge21)"));
			  	if (getcwd(Pname,sizeof(Pname)) == NULL)
				   fatal(gettext("getcwd() failed (ge20)"));
				if (chdir(Olddir) != 0)
				   fatal(gettext("cannot change directory (ge21)"));
				tp = trans(tp,Pname);
				tp = trans(tp,"/");
				tp = trans(tp,(sname(pkt->p_file)));
				break;
			case 'F':
				tp = trans(tp,pkt->p_file);
				break;
			case 'C':
				sprintf(str,"%d",pkt->p_glnno);
				tp = trans(tp,str);
				break;
			case 'A':
				tp = trans(tp,Zkeywd);
				tp = trans(tp,Type);
				tp = trans(tp," ");
				tp = trans(tp,Mod);
				tp = trans(tp," ");
				tp = trans(tp,Sid);
				tp = trans(tp,Zkeywd);
				break;
			default:
				str[0] = '%';
				str[1] = *lp;
				str[2] = '\0';
				tp = trans(tp,str);
				continue;
			}
			if (!(Sflags['i'-'a']))
				++Did_id;
			lp++;
		} else if (strncmp(lp, "%sccs.include.", 14) == 0) {
			register char *p;

			for (p = lp + 14; *p && *p != '%'; ++p);
			if (*p == '%') {
				*p = '\0';
				if (readcopy(lp + 14, tline))
					return(tline);
				*p = '%';
			}
			str[0] = *lp;
			str[1] = '\0';
			tp = trans(tp,str);
		} else {
			str[0] = *lp;
			str[1] = '\0';
			tp = trans(tp,str);
		}
	}
	return(tline);
}

static char *
_trans(tp,str,rest)
register char *tp, *str, *rest;
{
	register size_t filled_size	= tp - tline;
	register size_t new_size	= filled_size + strlen(str) + 1;

	if (new_size > tline_size) {
		tline_size = new_size + strlen(rest) + DEF_LINE_SIZE;
		tline = (char*) realloc(tline, tline_size);
		if (tline == NULL) {
			fatal(gettext("OUT OF SPACE (ut9)"));
		}
		tp = tline + filled_size;
	}
	while((*tp++ = *str++) != '\0')
		;
	return(tp-1);
}

#ifndef	HAVE_STRERROR
#define	strerror(p)		errmsgstr(p)	/* Use libschily version */
#endif
static int
readcopy(name, tp)
	register char *name;
	register char *tp;
{
	extern int errno;
	register FILE *fp = NULL;
	register int ch;
	char path[MAXPATHLEN];
	struct stat sb;
	register size_t filled_size	= tp - tline;
	register size_t new_size	= filled_size;

	if (!HADK && expand_IDs && (list_expand_IDs != NULL)) {
		if (!any('*', list_expand_IDs))
			return (0);
	}
	if (fp == NULL) {
		char *np;
		char *p = getenv(NOGETTEXT("SCCS_INCLUDEPATH"));

		if (p == NULL)
			p = INCPATH;
		do {
			for (np = p; *np; np++)
				if (*np == ':')
					break;
			(void)snprintf(path, sizeof (path), "%.*s/%s",
						(int)(np-p), p, name);
			if (p == np || *p == '\0')
				strlcpy(path, name, sizeof (path));
			fp = fopen(path, "r");
			p = np;
			if (*p == '\0')
				break;
			if (*p == ':')
				p++;
		} while (fp == NULL);
	}
	if (fp == NULL) {
		fprintf(stderr, gettext("WARNING: %s: %s.\n"),
						path, strerror(errno));
		return (0);
	}
	if (fstat(fileno(fp), &sb) < 0)
		return (0);
	new_size += sb.st_size + 1;

	if (new_size > tline_size) {
		tline_size = new_size + DEF_LINE_SIZE;
		tline = (char*) realloc(tline, tline_size);
		if (tline == NULL) {
			fatal(gettext("OUT OF SPACE (ut9)"));
		}
		tp = tline + filled_size;
	}

	while ((ch = getc(fp)) != EOF)
		*tp++ = ch;
	*tp++ = '\0';
	(void)fclose(fp);
	return (1);
}

static void
prfx(pkt)
register struct packet *pkt;
{
	char str[32];

	if (HADN)
		if (fprintf(pkt->p_gout, "%s\t", Mod) == EOF)
			xmsg(gfile, NOGETTEXT("prfx"));
	if (HADM) {
		sid_ba(&pkt->p_inssid,str);
		if (fprintf(pkt->p_gout, "%s\t", str) == EOF)
			xmsg(gfile, NOGETTEXT("prfx"));
	}
}

void
clean_up()
{
	/*
	clean_up is only called from fatal() upon bad termination.
	*/
	if (gpkt.p_iop) {
		fclose(gpkt.p_iop);
		gpkt.p_iop = NULL;
	}
	if (gpkt.p_gout) {
		fflush(gpkt.p_gout);
		gpkt.p_gout = NULL;
	}
	if (gpkt.p_gout && gpkt.p_gout != stdout) {
		fclose(gpkt.p_gout);
		gpkt.p_gout = NULL;
		unlink(Gfile);
	}
	if (HADE) {
	   uname(&un);
	   uuname = un.nodename;
	   if (mylock(auxf(gpkt.p_file,'z'), getpid(), uuname)) {
	      unlink(auxf(gpkt.p_file,'q'));
	      unlockit(auxf(gpkt.p_file,'z'), getpid(), uuname);
	   }
	}
	ffreeall();
}

static	char	warn[] = NOGETTEXT("WARNING: being edited: `%s' (ge18)\n");

static void
wrtpfile(pkt,inc,exc)
register struct packet *pkt;
char *inc, *exc;
{
	char line[64], str1[32], str2[32];
	char *user, *pfile;
	FILE *in, *out;
	struct pfile pf;
	extern time_t Timenow;
	int fd;

	if ((user=logname()) == NULL)
	   fatal(gettext("User ID not in password file (cm9)"));
	if ((fd=open(auxf(pkt->p_file,'q'),O_WRONLY|O_CREAT|O_EXCL|O_BINARY,0444)) < 0) {
	   fatal(gettext("cannot create lock file (cm4)"));
	}
	fchmod(fd, (mode_t)0644);
	out = fdfopen(fd, O_WRONLY|O_BINARY);
	if (exists(pfile = auxf(pkt->p_file,'p'))) {
		in = fdfopen(xopen(pfile, O_RDONLY|O_BINARY), O_RDONLY|O_BINARY);
		while (fgets(line,sizeof(line),in) != NULL) {
			if (fputs(line, out) == EOF)
			   xmsg(pfile, NOGETTEXT("wrtpfile"));
			pf_ab(line,&pf,0);
			if (!(Sflags[JOINTFLAG - 'a'])) {
				if ((pf.pf_gsid.s_rel == pkt->p_gotsid.s_rel &&
     				   pf.pf_gsid.s_lev == pkt->p_gotsid.s_lev &&
				   pf.pf_gsid.s_br == pkt->p_gotsid.s_br &&
				   pf.pf_gsid.s_seq == pkt->p_gotsid.s_seq) ||
				   (pf.pf_nsid.s_rel == pkt->p_reqsid.s_rel &&
				   pf.pf_nsid.s_lev == pkt->p_reqsid.s_lev &&
				   pf.pf_nsid.s_br == pkt->p_reqsid.s_br &&
				   pf.pf_nsid.s_seq == pkt->p_reqsid.s_seq)) {
					fclose(in);
					sprintf(SccsError,
					   gettext("being edited: `%s' (ge17)"),
					   line);
					fatal(SccsError);
				}
				if (!equal(pf.pf_user,user))
					fprintf(stderr,warn,line);
			} else {
			   fprintf(stderr,warn,line);
			}
		}
		fclose(in);
	}
	if (fseek(out,0L,2) == EOF)
		xmsg(pfile, NOGETTEXT("wrtpfile"));
	sid_ba(&pkt->p_gotsid,str1);
	sid_ba(&pkt->p_reqsid,str2);
	if (Timenow > Y2038)
		date_bal(&Timenow,line);	/* 4 digit year */
	else
		date_ba(&Timenow,line);		/* 2 digit year */
	if (fprintf(out,"%s %s %s %s",str1,str2,user,line) == EOF)
		xmsg(pfile, NOGETTEXT("wrtpfile"));
	if (inc)
		if (fprintf(out," -i%s",inc) == EOF)
			xmsg(pfile, NOGETTEXT("wrtpfile"));
	if (exc)
		if (fprintf(out," -x%s",exc) == EOF)
			xmsg(pfile, NOGETTEXT("wrtpfile"));
	if (cmrinsert () > 0)	/* if there are CMRS and they are okay */
		if (fprintf (out, " -z%s", cmr) == EOF)
			xmsg(pfile, NOGETTEXT("wrtpfile"));
	if (fprintf(out, "\n") == EOF)
		xmsg(pfile, NOGETTEXT("wrtpfile"));
	if (fflush(out) == EOF)
		xmsg(pfile, NOGETTEXT("wrtpfile"));
	if (fsync(fileno(out)) < 0)
		xmsg(pfile, NOGETTEXT("wrtpfile"));
	if (fclose(out) == EOF)
		xmsg(pfile, NOGETTEXT("wrtpfile"));
	if (pkt->p_verbose) {
                if (HADQ)
                   (void)fprintf(pkt->p_stdout, gettext("new version %s\n"),
				str2);
                else
                   (void)fprintf(pkt->p_stdout, gettext("new delta %s\n"),
				str2);
	}
}

/* Null routine to satisfy external reference from dodelt() */
/*ARGSUSED*/
void
escdodelt(pkt)
	struct packet *pkt;
{
}

/* NULL routine to satisfy external reference from dodelt() */
/*ARGSUSED*/
void
fredck(pkt)
	struct packet *pkt;
{
}

/* cmrinsert -- insert CMR numbers in the p.file. */

static int
cmrinsert ()
{
	char holdcmr[CMRLIMIT];
	char tcmr[CMRLIMIT];
	char *p;
	int bad;
	int isvalid;

	if (Sflags[CMFFLAG - 'a'] == 0)	{ /* CMFFLAG was not set. */
		return (0);
	}
	if (HADP && !HADZ) { /* no CMFFLAG and no place to prompt. */
		fatal(gettext("Background CASSI get with no CMRs\n"));
	}
retry:
	if (cmr[0] == '\0') {	/* No CMR list.  Make one. */
		if (HADZ && ((!isatty(0)) || (!isatty(1)))) {
		   fatal(gettext("Background CASSI get with invalid CMR\n"));
		}
		fprintf (stdout,
		   gettext("Input Comma Separated List of CMRs: "));
		fgets(cmr, CMRLIMIT, stdin);
		p=strend(cmr);
		*(--p) = '\0';
		if ((int)(p - cmr) == CMRLIMIT) {
		   fprintf(stdout, gettext("?Too many CMRs.\n"));
		   cmr[0] = '\0';
		   goto retry; /* Entry was too long. */
		}
	}
	/* Now, check the comma seperated list of CMRs for accuracy. */
	bad = 0;
	isvalid = 0;
	strcpy(tcmr, cmr);
	while ((p=strrchr(tcmr,',')) != NULL) {
		++p;
		if (cmrcheck(p,Sflags[CMFFLAG - 'a'])) {
			++bad;
		} else {
			++isvalid;
			strcat(holdcmr,",");
			strcat(holdcmr,p);
		}
		*(--p) = '\0';
	}
	if (*tcmr) {
		if (cmrcheck(tcmr,Sflags[CMFFLAG - 'a'])) {
			++bad;
		} else {
			++isvalid;
			strcat(holdcmr,",");
			strcat(holdcmr,tcmr);
		}
	}
	if (!bad && holdcmr[1]) {
	   strcpy(cmr,holdcmr+1);
	   return(1);
	} else {
	   if ((isatty(0)) && (isatty(1))) {
	      if (!isvalid)
		 fprintf(stdout,
		    gettext("Must enter at least one valid CMR.\n"));
	      else
		 fprintf(stdout,
		    gettext("Re-enter invalid CMRs, or press return.\n"));
	   }
	   cmr[0] = '\0';
	   goto retry;
	}
}
