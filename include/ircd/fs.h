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
#define HAVE_IRCD_FS_H

/*
 * Directory paths and filenames for UNIX systems.
 * IRCD_PREFIX is set using ./configure --prefix, see INSTALL.
 * Do not change these without corresponding changes in the build system.
 *
 * IRCD_PREFIX = prefix for all directories,
 * DPATH       = root directory of installation,
 * BINPATH     = directory for binary files,
 * ETCPATH     = directory for configuration files,
 * LOGPATH     = directory for logfiles,
 * MODPATH     = directory for modules,
 * AUTOMODPATH = directory for autoloaded modules
 */

#ifdef __cplusplus
inline namespace ircd  {
inline namespace fs    {

#define DPATH		IRCD_PREFIX
#define BINPATH		IRCD_PREFIX "/bin"
#define MODPATH		RB_MODULE_DIR
#define MODULE_DIR	RB_MODULE_DIR
#define AUTOMODPATH	RB_MODULE_DIR "/autoload"
#define ETCPATH		RB_ETC_DIR
#define LOGPATH		RB_LOG_DIR
#define UHPATH		RB_HELP_DIR "/users"
#define HPATH		RB_HELP_DIR "/opers"
#define SPATH		RB_BIN_DIR "/" BRANDING_NAME	/* ircd executable */
#define CPATH		ETCPATH "/ircd.conf"				/* ircd.conf file */
#define MPATH		ETCPATH "/ircd.motd"				/* MOTD file */
#define LPATH		LOGPATH "/ircd.log"				/* ircd logfile */
#define PPATH		PKGRUNDIR "/ircd.pid"				/* pid file */
#define OPATH		ETCPATH "/opers.motd"				/* oper MOTD file */
#define DBPATH		PKGLOCALSTATEDIR "/ban.db"			/* bandb file */

/* Below are the elements for default paths. */
enum path_index
{
	IRCD_PATH_PREFIX,
	IRCD_PATH_MODULES,
	IRCD_PATH_AUTOLOAD_MODULES,
	IRCD_PATH_ETC,
	IRCD_PATH_LOG,
	IRCD_PATH_USERHELP,
	IRCD_PATH_OPERHELP,
	IRCD_PATH_IRCD_EXEC,
	IRCD_PATH_IRCD_CONF,
	IRCD_PATH_IRCD_MOTD,
	IRCD_PATH_IRCD_LOG,
	IRCD_PATH_IRCD_PID,
	IRCD_PATH_IRCD_OMOTD,
	IRCD_PATH_BANDB,
	IRCD_PATH_BIN,
	IRCD_PATH_LIBEXEC,
	IRCD_PATH_COUNT
};

extern const char *paths[IRCD_PATH_COUNT];
extern const char *pathnames[IRCD_PATH_COUNT];

void relocate_paths();

} // namespace fs
} // namespace ircd
#endif // __cplusplus
