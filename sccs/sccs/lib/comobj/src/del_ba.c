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
 * Copyright 1994 Sun Microsystems, Inc. All rights reserved.
 * Use is subject to license terms.
 */
/*
 * This file contains modifications Copyright 2006-2011 J. Schilling
 *
 * @(#)del_ba.c	1.10 11/05/27 J. Schilling
 */
#if defined(sun)
#pragma ident "@(#)del_ba.c 1.10 11/05/27 J. Schilling"
#endif
/*
 * @(#)del_ba.c 1.3 06/12/12
 */

#if defined(sun)
#pragma ident	"@(#)del_ba.c"
#pragma ident	"@(#)sccs:lib/comobj/del_ba.c"
#endif
# include	<defines.h>


char *
del_ba(dt,str)
register struct deltab *dt;
char *str;
{
	register char *p;

	p = str;
	*p++ = CTLCHAR;
	*p++ = BDELTAB;
	*p++ = ' ';
	*p++ = dt->d_type;
	*p++ = ' ';
	p = sid_ba(&dt->d_sid,p);
	*p++ = ' ';
	if ((dt->d_datetime < Y1969) ||
	    (dt->d_datetime >= Y2038))
		date_bal(&dt->d_datetime,p);	/* 4 digit year */
	else
		date_ba(&dt->d_datetime,p);	/* 2 digit year */
	while (*p++)
		;
	--p;
	*p++ = ' ';
	copy(dt->d_pgmr,p);
	while (*p++)
		;
	--p;
	*p++ = ' ';
	sprintf(p,"%d",dt->d_serial);
	while (*p++)
		;
	--p;
	*p++ = ' ';
	sprintf(p,"%d",dt->d_pred);
	while (*p++)
		;
	--p;
	*p++ = '\n';
	*p = 0;
	return(str);
}
