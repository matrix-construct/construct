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
 *
 *  $Id: defaults.h 3354 2007-04-03 09:21:31Z nenolod $
 */

#ifndef INCLUDED_defaults_h
#define INCLUDED_defaults_h

/* this file is included (only) at the end of config.h, to supply default
 * values for things which are now configurable at runtime.
 */

/*
 * First, set other fd limits based on values from user
 */


#define MAXCONNECTIONS 65535 /* default max connections if getrlimit doesn't work */
/* class {} default values */
#define DEFAULT_SENDQ 20000000	/* default max SendQ */
#define PORTNUM 6667		/* default outgoing portnum */
#define DEFAULT_PINGFREQUENCY    120	/* Default ping frequency */
#define DEFAULT_CONNECTFREQUENCY 600	/* Default connect frequency */
#define TS_MAX_DELTA_MIN      10	/* min value for ts_max_delta */
#define TS_MAX_DELTA_DEFAULT  600	/* default for ts_max_delta */
#define TS_WARN_DELTA_MIN     10	/* min value for ts_warn_delta */
#define TS_WARN_DELTA_DEFAULT 30	/* default for ts_warn_delta */
/* ServerInfo default values */
#define NETWORK_NAME_DEFAULT "EFnet"	/* default for network_name */
#define NETWORK_DESC_DEFAULT "Eris Free Network"	/* default for network_desc */
/* General defaults */
#define CLIENT_FLOOD_DEFAULT 20	/* default for client_flood */
#define CLIENT_FLOOD_MAX     2000
#define CLIENT_FLOOD_MIN     10
#define LINKS_DELAY_DEFAULT  300
#define MAX_TARGETS_DEFAULT 4	/* default for max_targets */
#define IDENT_TIMEOUT 10
#define MIN_JOIN_LEAVE_TIME  60
#define MAX_JOIN_LEAVE_COUNT  25
#define OPER_SPAM_COUNTDOWN   5
#define JOIN_LEAVE_COUNT_EXPIRE_TIME 120
#define MIN_SPAM_NUM 5
#define MIN_SPAM_TIME 60
#define CONFIG_RATBOX_LEVEL_1
#endif				/* INCLUDED_defaults_h */
