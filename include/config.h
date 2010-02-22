/*
 *  ircd-ratbox: A slightly useful ircd.
 *  config.h: The ircd compile-time-configurable header.
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
 *  $Id: config.h 3354 2007-04-03 09:21:31Z nenolod $
 */

#ifndef INCLUDED_config_h
#define INCLUDED_config_h

#include "setup.h"

/* 
 * Directory paths and filenames for UNIX systems.
 * IRCD_PREFIX is set using ./configure --prefix, see INSTALL.
 * The other defaults should be fine.
 *
 * NOTE: CHANGING THESE WILL NOT ALTER THE DIRECTORY THAT FILES WILL
 *       BE INSTALLED TO.  IF YOU CHANGE THESE, DO NOT USE MAKE INSTALL,
 *       BUT COPY THE FILES MANUALLY TO WHERE YOU WANT THEM.
 *
 * IRCD_PREFIX = prefix for all directories,
 * DPATH       = root directory of installation,
 * BINPATH     = directory for binary files,
 * ETCPATH     = directory for configuration files,
 * LOGPATH     = directory for logfiles,
 * MODPATH     = directory for modules,
 * AUTOMODPATH = directory for autoloaded modules
 */

/* dirs */
#define DPATH   IRCD_PREFIX
#define BINPATH IRCD_PREFIX "/bin/"
#define LIBPATH IRCD_PREFIX "/lib/"
#define MODPATH MODULE_DIR
#define AUTOMODPATH MODULE_DIR "/autoload/"
#define ETCPATH ETC_DIR 
#define LOGPATH LOG_DIR
#define UHPATH   HELP_DIR "/users"
#define HPATH  HELP_DIR "/opers"

/* files */
#define SPATH    BINPATH "/ircd"		   /* ircd executable */
#define LIPATH   LIBPATH "/libircd" SHARED_SUFFIX  /* ircd library */
#define CPATH    ETCPATH "/ircd.conf"		   /* ircd.conf file */
#define MPATH    ETCPATH "/ircd.motd"		   /* MOTD file */
#define LPATH    LOGPATH "/ircd.log"		   /* ircd logfile */
#define PPATH    ETCPATH "/ircd.pid"		   /* pid file */
#define OPATH    ETCPATH "/opers.motd"		   /* oper MOTD file */
#define DBPATH   ETCPATH "/ban.db"                 /* bandb file */

/* IGNORE_BOGUS_TS
 * Ignore bogus timestamps from other servers. Yes this will desync
 * the network, but it will allow chanops to resync with a valid non TS 0
 *
 * This should be enabled network wide, or not at all.
 */
#undef  IGNORE_BOGUS_TS

/* HANGONGOODLINK and HANGONRETRYDELAY
 * Often net breaks for a short time and it's useful to try to
 * establishing the same connection again faster than CONNECTFREQUENCY
 * would allow. But, to keep trying on bad connection, we require
 * that connection has been open for certain minimum time
 * (HANGONGOODLINK) and we give the net few seconds to steady
 * (HANGONRETRYDELAY). This latter has to be long enough that the
 * other end of the connection has time to notice it broke too.
 * 1997/09/18 recommended values by ThemBones for modern EFnet
 */
#define HANGONRETRYDELAY 60	/* Recommended value: 30-60 seconds */
#define HANGONGOODLINK 3600	/* Recommended value: 30-60 minutes */

/* KILLCHASETIMELIMIT -
 * Max time from the nickname change that still causes KILL
 * automatically to switch for the current nick of that user. (seconds)
 */
#define KILLCHASETIMELIMIT 90	/* Recommended value: 90 */

/* RATBOX_SOMAXCONN
 * Use SOMAXCONN if OS has it, otherwise use this value for the 
 * listen(); backlog.  5 for AIX/SUNOS, 25 for other OSs.
 */
#define RATBOX_SOMAXCONN 25

/* MAX_BUFFER
 * The amount of fds to reserve for clients exempt from limits
 * and dns lookups.
 */
#define MAX_BUFFER      60

/* ----------------------------------------------------------------
 * STOPSTOPSTOPSTOPSTOPSTOPSTOPSTOPSTOPSTOPSTOPSTOPSTOPSTOPSTOPSTOP
 * ----------------------------------------------------------------
 * The options below this line should NOT be modified.
 * ----------------------------------------------------------------
 */

#define CONFIG_RATBOX_LEVEL_2

#include "defaults.h"
#endif /* INCLUDED_config_h */
