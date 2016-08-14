/*
 *  charybdis: A slightly useful ircd.
 *  ircd.c: Starts up and runs the ircd.
 *
 *  Copyright (C) 1990 Jarkko Oikarinen and University of Oulu, Co Center
 *  Copyright (C) 1996-2002 Hybrid Development Team
 *  Copyright (C) 2002-2008 ircd-ratbox development team
 *  Copyright (C) 2005-2013 charybdis development team
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

#include <ircd/stdinc.h>
#include <ircd/logger.h>

namespace fs = ircd::fs;
using namespace fs;

auto paths = []
{
	using ircd::util::num_of;
	using namespace fs::path;

	std::array<const char *, num_of<fs::path::index>()> paths;

	paths[PREFIX]             = DPATH;
	paths[MODULES]            = MODPATH;
	paths[AUTOLOAD_MODULES]   = AUTOMODPATH;
	paths[ETC]                = ETCPATH;
	paths[LOG]                = LOGPATH;
	paths[USERHELP]           = UHPATH;
	paths[OPERHELP]           = HPATH;
	paths[IRCD_EXEC]          = SPATH;
	paths[IRCD_CONF]          = CPATH;
	paths[IRCD_MOTD]          = MPATH;
	paths[IRCD_LOG]           = LPATH;
	paths[IRCD_PID]           = PPATH;
	paths[IRCD_OMOTD]         = OPATH;
	paths[BANDB]              = DBPATH;
	paths[BIN]                = BINPATH;
	paths[LIBEXEC]            = PKGLIBEXECDIR;

	return paths;
}();

auto pathnames = []
{
	using ircd::util::num_of;
	using namespace fs::path;

	std::array<const char *, num_of<fs::path::index>()> names;

	names[PREFIX]             = "prefix";
	names[MODULES]            = "modules";
	names[AUTOLOAD_MODULES]   = "autoload modules";
	names[ETC]                = "config";
	names[LOG]                = "log";
	names[USERHELP]           = "user help";
	names[OPERHELP]           = "oper help";
	names[IRCD_EXEC]          = "ircd binary";
	names[IRCD_CONF]          = "ircd.conf";
	names[IRCD_MOTD]          = "ircd.motd";
	names[IRCD_LOG]           = "ircd.log";
	names[IRCD_PID]           = "ircd.pid";
	names[IRCD_OMOTD]         = "oper motd";
	names[BANDB]              = "bandb";
	names[BIN]                = "binary dir";
	names[LIBEXEC]            = "libexec dir";

	return names;
}();

const char *
fs::path::get(index index)
noexcept try
{
	return paths.at(index);
}
catch(const std::out_of_range &e)
{
	return nullptr;
}

const char *
fs::path::name(index index)
noexcept try
{
	return pathnames.at(index);
}
catch(const std::out_of_range &e)
{
	return nullptr;
}

#ifdef _WIN32
static
void relocate()
{
	using namespace fs::path;

	char prefix[PATH_MAX], workbuf[PATH_MAX];
	char *p;

	rb_strlcpy(prefix, rb_path_to_self(), sizeof prefix);

	paths[IRCD_EXEC] = rb_strdup(prefix);

	/* if we're running from inside the source tree, we probably do not want to relocate any other paths */
	if (strstr(prefix, ".libs") != NULL)
		return;

	/* prefix = /home/kaniini/ircd/bin/ircd */
	p = strrchr(prefix, RB_PATH_SEPARATOR);
	if (rb_unlikely(p == NULL))
		return;
	*p = 0;

	/* prefix = /home/kaniini/ircd/bin */
	p = strrchr(prefix, RB_PATH_SEPARATOR);
	if (rb_unlikely(p == NULL))
		return;
	*p = 0;

	/* prefix = /home/kaniini/ircd */
	paths[PREFIX] = rb_strdup(prefix);

	/* now that we have our prefix, we can relocate the other paths... */
	snprintf(workbuf, sizeof workbuf, "%s%cmodules", prefix, RB_PATH_SEPARATOR);
	paths[MODULES] = rb_strdup(workbuf);

	snprintf(workbuf, sizeof workbuf, "%s%cmodules%cautoload", prefix, RB_PATH_SEPARATOR, RB_PATH_SEPARATOR);
	paths[AUTOLOAD_MODULES] = rb_strdup(workbuf);

	snprintf(workbuf, sizeof workbuf, "%s%cetc", prefix, RB_PATH_SEPARATOR);
	paths[ETC] = rb_strdup(workbuf);

	snprintf(workbuf, sizeof workbuf, "%s%clog", prefix, RB_PATH_SEPARATOR);
	paths[LOG] = rb_strdup(workbuf);

	snprintf(workbuf, sizeof workbuf, "%s%chelp%cusers", prefix, RB_PATH_SEPARATOR, RB_PATH_SEPARATOR);
	paths[USERHELP] = rb_strdup(workbuf);

	snprintf(workbuf, sizeof workbuf, "%s%chelp%copers", prefix, RB_PATH_SEPARATOR, RB_PATH_SEPARATOR);
	paths[OPERHELP] = rb_strdup(workbuf);

	snprintf(workbuf, sizeof workbuf, "%s%cetc%circd.conf", prefix, RB_PATH_SEPARATOR, RB_PATH_SEPARATOR);
	paths[IRCD_CONF] = rb_strdup(workbuf);

	snprintf(workbuf, sizeof workbuf, "%s%cetc%circd.motd", prefix, RB_PATH_SEPARATOR, RB_PATH_SEPARATOR);
	paths[IRCD_MOTD] = rb_strdup(workbuf);

	snprintf(workbuf, sizeof workbuf, "%s%cetc%copers.motd", prefix, RB_PATH_SEPARATOR, RB_PATH_SEPARATOR);
	paths[IRCD_OMOTD] = rb_strdup(workbuf);

	snprintf(workbuf, sizeof workbuf, "%s%cetc%cban.db", prefix, RB_PATH_SEPARATOR, RB_PATH_SEPARATOR);
	paths[BANDB] = rb_strdup(workbuf);

	snprintf(workbuf, sizeof workbuf, "%s%cetc%circd.pid", prefix, RB_PATH_SEPARATOR, RB_PATH_SEPARATOR);
	paths[IRCD_PID] = rb_strdup(workbuf);

	snprintf(workbuf, sizeof workbuf, "%s%clogs%circd.log", prefix, RB_PATH_SEPARATOR, RB_PATH_SEPARATOR);
	paths[IRCD_LOG] = rb_strdup(workbuf);

	snprintf(workbuf, sizeof workbuf, "%s%cbin", prefix, RB_PATH_SEPARATOR);
	paths[BIN] = rb_strdup(workbuf);
	paths[LIBEXEC] = rb_strdup(workbuf);

	inotice("runtime paths:");
	for (size_t i(0); i < paths.size() && i < pathnames.size(); ++i)
		inotice("  %s: %s", pathnames[i], paths[i]);
}
#else
static
void relocate()
{
}
#endif

void fs::init()
{
	relocate();
}
