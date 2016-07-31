/*
 *  ircd-ratbox: A slightly useful ircd.
 *  defaults.h: The ircd defaults header.
 *
 *  Copyright (C) 1990 Jarkko Oikarinen and University of Oulu, Co Center
 *  Copyright (C) 1996-2002 Hybrid Development Team
 *  Copyright (C) 2002-2004 ircd-ratbox development team
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
 */

#pragma once
#define HAVE_IRCD_DEFAULTS_H

/* /!\ DANGER WILL ROBINSON! DANGER! /!\
 *
 * Do not mess with these values unless you know what you are doing!
 */

/* The below are used as defaults if not found in the configuration file (or on ircd warm-up).
 * Don't change these - edit the conf file instead.
 */
#ifdef __cplusplus
namespace ircd     {
namespace defaults {

enum general
{
	CLIENT_FLOOD_DEFAULT = 20,              // default for client_flood
	CLIENT_FLOOD_MAX = 2000,
	CLIENT_FLOOD_MIN = 10,
	LINKS_DELAY_DEFAULT = 300,
	MAX_TARGETS_DEFAULT = 4,                // default for max_targets */
	IDENT_TIMEOUT_DEFAULT = 5,
	BLACKLIST_TIMEOUT_DEFAULT = 10,
	OPM_TIMEOUT_DEFAULT = 10,
	RDNS_TIMEOUT_DEFAULT = 5,
	MIN_JOIN_LEAVE_TIME = 60,
	MAX_JOIN_LEAVE_COUNT = 25,
	OPER_SPAM_COUNTDOWN = 5,
	JOIN_LEAVE_COUNT_EXPIRE_TIME = 120,
	MIN_SPAM_NUM = 5,
	MIN_SPAM_TIME = 60,
	HOSTLEN = 63,                           // Length of hostname.
};

enum iline
{
	DEFAULT_SENDQ = 20000000,               // default max SendQ
	PORTNUM = 6667,                         // default outgoing portnum
	DEFAULT_PINGFREQUENCY = 120,            // Default ping frequency
	DEFAULT_CONNECTFREQUENCY = 600,         // Default connect frequency
	TS_MAX_DELTA_MIN = 10,                  // min value for ts_max_delta
	TS_MAX_DELTA_DEFAULT = 600,             // default for ts_max_delta
	TS_WARN_DELTA_MIN = 10,                 // min value for ts_warn_delta
	TS_WARN_DELTA_DEFAULT = 30,             // default for ts_warn_delta
};

enum len
{
	IRCD_RES_HOSTLEN = 255,                 // Longest hostname we're willing to work with due to DNSBLs this is more than HOSTLEN.
	USERLEN = 10,
	REALLEN = 50,
	CHANNELLEN = 200,
	LOC_CHANNELLEN = 50,

	// reason length of klines, parts, quits etc
	/* for quit messages, note that a client exit server notice
	 * :012345678901234567890123456789012345678901234567890123456789123 NOTICE * :*** Notice -- Client exiting: 012345678901234567 (0123456789@012345678901234567890123456789012345678901234567890123456789123) [] [1111:2222:3333:4444:5555:6666:7777:8888]
	 * takes at most 246 bytes (including CRLF and '\0') and together with the
	 * quit reason should fit in 512 */
	REASONLEN = 260,                        // kick/part/quit
	BANREASONLEN = 390,                     // kline/dline
	AWAYLEN = TOPICLEN,
	KILLLEN = 200,                          // with Killed (nick ()) added this should fit in quit

	KEYLEN = 24,                            // 23+1 for \0
	OPERNICKLEN = (NICKLEN*2),              // Length of OPERNICKs.

	USERHOST_REPLYLEN = (NICKLEN+HOSTLEN+USERLEN+5),
	MAX_DATE_STRING = 32,                   // maximum string length for a date string

	HELPLEN = 400,
};


#define BUFSIZE 512 /* WARNING: *DONT* CHANGE THIS!!!! */
#define MAXCONNECTIONS 65536  /* default max connections if getrlimit doesn't work */
#define NETWORK_NAME_DEFAULT "DefaultNet" /* default for network_name */


/*
 * message return values
 */
#define CLIENT_EXITED    -2
#define CLIENT_PARSE_ERROR -1
#define CLIENT_OK 1

/* Read buffer size */
#define READBUF_SIZE 16384


/* Below are somewhat configurable settings (though it's probably a bad idea
 * to blindly mess with them). If in any doubt, leave them alone.
 */

/* HANGONGOODLINK and HANGONRETRYDELAY
 * Often net breaks for a short time and it's useful to try to
 * establishing the same connection again faster than CONNECTFREQUENCY
 * would allow. But, to keep trying on bad connection, we require
 * that connection has been open for certain minimum time
 * (HANGONGOODLINK) and we give the net few seconds to steady
 * (HANGONRETRYDELAY). This latter has to be long enough that the
 * other end of the connection has time to notice it broke too.
 */
#define HANGONRETRYDELAY 60 /* Recommended value: 30-60 seconds */
#define HANGONGOODLINK  3600 /* Recommended value: 30-60 minutes */

/* KILLCHASETIMELIMIT -
 * Max time from the nickname change that still causes KILL
 * automatically to switch for the current nick of that user. (seconds)
 */
#define KILLCHASETIMELIMIT 90 /* Recommended value: 90 */

/* MAX_BUFFER
 * The amount of fds to reserve for clients exempt from limits
 * and dns lookups.
 */
#define MAX_BUFFER  60

/*
 * Use SOMAXCONN if OS has it, otherwise use this value for the
 * listen(); backlog.  5 for AIX/SUNOS, 25 for other OSs.
 */
#ifndef SOMAXCONN
# define SOMAXCONN 25
#endif

} // namespace defaults
} // namespace ircd
#endif // __cplusplus
