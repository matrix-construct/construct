/*
 * Copyright (C) 2016 Charybdis Development Team
 * Copyright (C) 2016 Jason Volk <jason@zemos.net>
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice is present in all copies.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once
#define HAVE_IRCD_PATH_H

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

std::string build(const std::initializer_list<std::string> &);

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
