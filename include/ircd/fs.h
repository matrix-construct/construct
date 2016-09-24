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
namespace ircd {
namespace path {

IRCD_EXCEPTION(ircd::error, error)
IRCD_EXCEPTION(error, filesystem_error)

constexpr auto DPATH = IRCD_PREFIX;
constexpr auto BINPATH = IRCD_PREFIX "/bin";
constexpr auto MODPATH = RB_MODULE_DIR;
constexpr auto ETCPATH = RB_ETC_DIR;
constexpr auto LOGPATH = RB_LOG_DIR;
constexpr auto UHPATH = RB_HELP_DIR "/users";
constexpr auto HPATH = RB_HELP_DIR "/opers";
constexpr auto SPATH = RB_BIN_DIR "/" BRANDING_NAME;         // ircd executable
constexpr auto CPATH = RB_ETC_DIR "/ircd.conf";              // ircd.conf file
constexpr auto MPATH = RB_ETC_DIR "/ircd.motd";              // MOTD file
constexpr auto LPATH = RB_LOG_DIR "/ircd.log";               // ircd logfile
constexpr auto OPATH = RB_ETC_DIR "/opers.motd";             // oper MOTD file
constexpr auto DBPATH = PKGLOCALSTATEDIR;                    // database prefix
constexpr auto BDBPATH = PKGLOCALSTATEDIR "/ban.db";         // bandb file

// Below are the elements for default paths.
enum index
{
	PREFIX,
	BIN,
	ETC,
	LOG,
	LIBEXEC,
	MODULES,
	USERHELP,
	OPERHELP,
	IRCD_CONF,
	IRCD_EXEC,
	IRCD_MOTD,
	IRCD_LOG,
	IRCD_OMOTD,
	BANDB,
	DB,

	_NUM_
};

const char *get(index) noexcept;
const char *name(index) noexcept;

bool exists(const std::string &path);
bool is_dir(const std::string &path);
bool is_reg(const std::string &path);

std::vector<std::string> ls(const std::string &path);
std::vector<std::string> ls_recursive(const std::string &path);

std::string cwd();
void chdir(const std::string &path);

} // namespace path
} // namespace ircd
#endif // __cplusplus
