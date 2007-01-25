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
 *  $Id: ircd_defs.h 865 2006-02-16 14:05:37Z nenolod $
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


#include "s_log.h"
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

/*
 * This defines the version of the data structures used in the ircd.
 * In the event of a mismatch (i.e. this is incremented due to a major
 * change that cannot be accomidated for in the ircd), then a hard
 * restart occurs.
 */
#define CHARYBDIS_DV	0x00010200	/* 1.2.0 */

#define HOSTLEN         63	/* Length of hostname.  Updated to         */
				/* comply with RFC1123                     */

#define USERLEN         10
#define REALLEN         50
#define CHANNELLEN      200
#define LOC_CHANNELLEN	50

/* reason length of klines, parts, quits etc */
#define REASONLEN	TOPICLEN	/* in charybdis, reasonlen is controlled via topiclen */
#define AWAYLEN		TOPICLEN	/* ditto for awaylen */
#define KILLLEN         TOPICLEN	/* and killlen. have a nice day. --nenolod */

/* 23+1 for \0 */
#define KEYLEN          24
#define BUFSIZE         512	/* WARNING: *DONT* CHANGE THIS!!!! */
#define MAXRECIPIENTS   20
#define MAXBANLENGTH    1024
#define OPERNICKLEN     NICKLEN*2	/* Length of OPERNICKs. */

#define USERHOST_REPLYLEN       (NICKLEN+HOSTLEN+USERLEN+5)
#define MAX_DATE_STRING 32	/* maximum string length for a date string */

#define HELPLEN         400

/* 
 * message return values 
 */
#define CLIENT_EXITED    -2
#define CLIENT_PARSE_ERROR -1
#define CLIENT_OK	1

#ifdef IPV6
#ifndef AF_INET6
#error "AF_INET6 not defined"
#endif


#else /* #ifdef IPV6 */

#ifndef AF_INET6
#define AF_INET6 AF_MAX		/* Dummy AF_INET6 declaration */
#endif
#endif /* #ifdef IPV6 */


#ifdef IPV6
#define irc_sockaddr_storage sockaddr_storage
#else
#define irc_sockaddr_storage sockaddr
#define ss_family sa_family
#ifdef SOCKADDR_IN_HAS_LEN
#define ss_len sa_len
#endif
#endif

#ifdef IPV6
#define PATRICIA_BITS	128
#else
#define PATRICIA_BITS	32
#endif

#ifdef SOCKADDR_IN_HAS_LEN
#define SET_SS_LEN(x, y) (x).ss_len = (y)
#define GET_SS_LEN(x) x.ss_len
#else
#define SET_SS_LEN(x, y)
#ifdef IPV6
#define GET_SS_LEN(x) x.ss_family == AF_INET ? sizeof(struct sockaddr_in) : sizeof(struct sockaddr_in6)
#else
#define GET_SS_LEN(x) sizeof(struct sockaddr_in)
#endif
#endif


#endif /* INCLUDED_ircd_defs_h */
