/*
 *  charybdis: An advanced IRCd.
 *  ircd_defs.h: A header for ircd global definitions.
 *
 *  Copyright (C) 1990 Jarkko Oikarinen and University of Oulu, Co Center
 *  Copyright (C) 1996-2002 Hybrid Development Team
 *  Copyright (C) 2002-2004 ircd-ratbox development team
 *  Copyright (C) 2005-2006 Charybdis development team
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307
 *  USA
 *
 *  $Id: ircd_defs.h 3512 2007-06-06 16:20:40Z nenolod $
 */

/*
 * NOTE: NICKLEN and TOPICLEN do not live here anymore. Set it with configure
 * Otherwise there are no user servicable part here. 
 *
 */

/* ircd_defs.h - Global size definitions for record entries used
 * througout ircd. Please think 3 times before adding anything to this
 * file.
 */
#ifndef INCLUDED_ircd_defs_h
#define INCLUDED_ircd_defs_h

#include "config.h"

/* For those unfamiliar with GNU format attributes, a is the 1 based
 * argument number of the format string, and b is the 1 based argument
 * number of the variadic ... */
#ifdef __GNUC__
#define AFP(a,b) __attribute__((format (printf, a, b)))
#else
#define AFP(a,b)
#endif

/*
 * This ensures that __attribute__((deprecated)) is not used in for example
 * sun CC, since it's a GNU-specific extension. -nenolod
 */
#ifdef __GNUC__
#define IRC_DEPRECATED __attribute__((deprecated))
#else
#define IRC_DEPRECATED
#endif

#include "logger.h"
#include "send.h"

#ifdef SOFT_ASSERT
#ifdef __GNUC__
#define s_assert(expr)	do								\
			if(!(expr)) {							\
				ilog(L_MAIN, 						\
				"file: %s line: %d (%s): Assertion failed: (%s)",	\
				__FILE__, __LINE__, __PRETTY_FUNCTION__, #expr); 	\
				sendto_realops_snomask(SNO_GENERAL, L_ALL, 			\
				"file: %s line: %d (%s): Assertion failed: (%s)",	\
				__FILE__, __LINE__, __PRETTY_FUNCTION__, #expr);	\
			}								\
			while(0)
#else
#define s_assert(expr)	do								\
			if(!(expr)) {							\
				ilog(L_MAIN, 						\
				"file: %s line: %d: Assertion failed: (%s)",		\
				__FILE__, __LINE__, #expr); 				\
				sendto_realops_snomask(SNO_GENERAL, L_ALL,			\
				"file: %s line: %d: Assertion failed: (%s)"		\
				__FILE__, __LINE__, #expr);				\
			}								\
			while(0)
#endif
#else
#define s_assert(expr)	assert(expr)
#endif

#if !defined(CONFIG_RATBOX_LEVEL_1)
#  error Incorrect config.h for this revision of ircd.
#endif

#define HOSTLEN         63	/* Length of hostname.  Updated to         */
				/* comply with RFC1123                     */

#define USERLEN         10
#define REALLEN         50
#define CHANNELLEN      200
#define LOC_CHANNELLEN	50

/* reason length of klines, parts, quits etc */
/* for quit messages, note that a client exit server notice
 * :012345678901234567890123456789012345678901234567890123456789123 NOTICE * :*** Notice -- Client exiting: 012345678901234567 (0123456789@012345678901234567890123456789012345678901234567890123456789123) [] [1111:2222:3333:4444:5555:6666:7777:8888]
 * takes at most 246 bytes (including CRLF and '\0') and together with the
 * quit reason should fit in 512 */
#define REASONLEN	260	/* kick/part/quit */
#define BANREASONLEN	390	/* kline/dline */
#define AWAYLEN		TOPICLEN
#define KILLLEN         200	/* with Killed (nick ()) added this should fit in quit */

/* 23+1 for \0 */
#define KEYLEN          24
#define BUFSIZE         512	/* WARNING: *DONT* CHANGE THIS!!!! */
#define OPERNICKLEN     (NICKLEN*2)	/* Length of OPERNICKs. */

#define USERHOST_REPLYLEN       (NICKLEN+HOSTLEN+USERLEN+5)
#define MAX_DATE_STRING 32	/* maximum string length for a date string */

#define HELPLEN         400

/* 
 * message return values 
 */
#define CLIENT_EXITED    -2
#define CLIENT_PARSE_ERROR -1
#define CLIENT_OK	1

#ifdef RB_IPV6
#ifndef AF_INET6
#error "AF_INET6 not defined"
#endif


#else /* #ifdef RB_IPV6 */

#ifndef AF_INET6
#define AF_INET6 AF_MAX		/* Dummy AF_INET6 declaration */
#endif
#endif /* #ifdef RB_IPV6 */

#ifdef RB_IPV6
#define PATRICIA_BITS	128
#else
#define PATRICIA_BITS	32
#endif

#endif /* INCLUDED_ircd_defs_h */
