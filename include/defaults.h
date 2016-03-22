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

#ifndef INCLUDED_defaults_h
#define INCLUDED_defaults_h

/* /!\ DANGER WILL ROBINSON! DANGER! /!\
 *
 * Do not mess with these values unless you know what you are doing!
 */

#include "setup.h"

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
/* General defaults */
#define CLIENT_FLOOD_DEFAULT 20	/* default for client_flood */
#define CLIENT_FLOOD_MAX     2000
#define CLIENT_FLOOD_MIN     10
#define LINKS_DELAY_DEFAULT  300
#define MAX_TARGETS_DEFAULT 4	/* default for max_targets */
#define IDENT_TIMEOUT_DEFAULT 5
#define MIN_JOIN_LEAVE_TIME  60
#define MAX_JOIN_LEAVE_COUNT  25
#define OPER_SPAM_COUNTDOWN   5
#define JOIN_LEAVE_COUNT_EXPIRE_TIME 120
#define MIN_SPAM_NUM 5
#define MIN_SPAM_TIME 60

/*
 * Directory paths and filenames for UNIX systems.
 * IRCD_PREFIX is set using ./configure --prefix, see INSTALL.
 *
 * IRCD_PREFIX = prefix for all directories,
 * DPATH       = root directory of installation,
 * BINPATH     = directory for binary files,
 * ETCPATH     = directory for configuration files,
 * LOGPATH     = directory for logfiles,
 * MODPATH     = directory for modules,
 * AUTOMODPATH = directory for autoloaded modules
 */

#define DPATH   IRCD_PREFIX
#define BINPATH IRCD_PREFIX "/bin/"
#define MODPATH MODULE_DIR
#define AUTOMODPATH MODULE_DIR "/autoload/"
#define ETCPATH ETC_DIR
#define LOGPATH LOG_DIR
#define UHPATH   HELP_DIR "/users"
#define HPATH  HELP_DIR "/opers"
#define SPATH    BINPATH "/" PROGRAM_PREFIX "charybdis"		   /* ircd executable */
#define CPATH    ETCPATH "/ircd.conf"		                   /* ircd.conf file */
#define MPATH    ETCPATH "/ircd.motd"		                   /* MOTD file */
#define LPATH    LOGPATH "/ircd.log"		                   /* ircd logfile */
#define PPATH    PKGRUNDIR "/ircd.pid"		                   /* pid file */
#define OPATH    ETCPATH "/opers.motd"		                   /* oper MOTD file */
#define DBPATH   PKGLOCALSTATEDIR "/ban.db"                        /* bandb file */

/* HANGONGOODLINK and HANGONRETRYDELAY
 * Often net breaks for a short time and it's useful to try to
 * establishing the same connection again faster than CONNECTFREQUENCY
 * would allow. But, to keep trying on bad connection, we require
 * that connection has been open for certain minimum time
 * (HANGONGOODLINK) and we give the net few seconds to steady
 * (HANGONRETRYDELAY). This latter has to be long enough that the
 * other end of the connection has time to notice it broke too.
 */
#define HANGONRETRYDELAY 60	/* Recommended value: 30-60 seconds */
#define HANGONGOODLINK 3600	/* Recommended value: 30-60 minutes */

/* KILLCHASETIMELIMIT -
 * Max time from the nickname change that still causes KILL
 * automatically to switch for the current nick of that user. (seconds)
 */
#define KILLCHASETIMELIMIT 90	/* Recommended value: 90 */

/* MAX_BUFFER
 * The amount of fds to reserve for clients exempt from limits
 * and dns lookups.
 */
#define MAX_BUFFER      60

/* CHARYBDIS_SOMAXCONN
 * Use SOMAXCONN if OS has it, otherwise use this value for the
 * listen(); backlog.  5 for AIX/SUNOS, 25 for other OSs.
 */
#define CHARYBDIS_SOMAXCONN 25

#endif				/* INCLUDED_defaults_h */
