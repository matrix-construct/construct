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

using namespace ircd;


const char *fs::paths[IRCD_PATH_COUNT] =
{
	[IRCD_PATH_PREFIX] = DPATH,
	[IRCD_PATH_MODULES] = MODPATH,
	[IRCD_PATH_AUTOLOAD_MODULES] = AUTOMODPATH,
	[IRCD_PATH_ETC] = ETCPATH,
	[IRCD_PATH_LOG] = LOGPATH,
	[IRCD_PATH_USERHELP] = UHPATH,
	[IRCD_PATH_OPERHELP] = HPATH,
	[IRCD_PATH_IRCD_EXEC] = SPATH,
	[IRCD_PATH_IRCD_CONF] = CPATH,
	[IRCD_PATH_IRCD_MOTD] = MPATH,
	[IRCD_PATH_IRCD_LOG] = LPATH,
	[IRCD_PATH_IRCD_PID] = PPATH,
	[IRCD_PATH_IRCD_OMOTD] = OPATH,
	[IRCD_PATH_BANDB] = DBPATH,
	[IRCD_PATH_BIN] = BINPATH,
	[IRCD_PATH_LIBEXEC] = PKGLIBEXECDIR,
};

const char *fs::pathnames[IRCD_PATH_COUNT] =
{
	[IRCD_PATH_PREFIX] = "prefix",
	[IRCD_PATH_MODULES] = "modules",
	[IRCD_PATH_AUTOLOAD_MODULES] = "autoload modules",
	[IRCD_PATH_ETC] = "config",
	[IRCD_PATH_LOG] = "log",
	[IRCD_PATH_USERHELP] = "user help",
	[IRCD_PATH_OPERHELP] = "oper help",
	[IRCD_PATH_IRCD_EXEC] = "ircd binary",
	[IRCD_PATH_IRCD_CONF] = "ircd.conf",
	[IRCD_PATH_IRCD_MOTD] = "ircd.motd",
	[IRCD_PATH_IRCD_LOG] = "ircd.log",
	[IRCD_PATH_IRCD_PID] = "ircd.pid",
	[IRCD_PATH_IRCD_OMOTD] = "oper motd",
	[IRCD_PATH_BANDB] = "bandb",
	[IRCD_PATH_BIN] = "binary dir",
	[IRCD_PATH_LIBEXEC] = "libexec dir",
};


/*
 * relocate_paths
 *
 * inputs       - none
 * output       - none
 * side effects - items in paths[] array are relocated
 */
#ifdef _WIN32
void fs::relocate_paths()
{
	char prefix[PATH_MAX], workbuf[PATH_MAX];
	char *p;

	rb_strlcpy(prefix, rb_path_to_self(), sizeof prefix);

	paths[IRCD_PATH_IRCD_EXEC] = rb_strdup(prefix);

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
	paths[IRCD_PATH_PREFIX] = rb_strdup(prefix);

	/* now that we have our prefix, we can relocate the other paths... */
	snprintf(workbuf, sizeof workbuf, "%s%cmodules", prefix, RB_PATH_SEPARATOR);
	paths[IRCD_PATH_MODULES] = rb_strdup(workbuf);

	snprintf(workbuf, sizeof workbuf, "%s%cmodules%cautoload", prefix, RB_PATH_SEPARATOR, RB_PATH_SEPARATOR);
	paths[IRCD_PATH_AUTOLOAD_MODULES] = rb_strdup(workbuf);

	snprintf(workbuf, sizeof workbuf, "%s%cetc", prefix, RB_PATH_SEPARATOR);
	paths[IRCD_PATH_ETC] = rb_strdup(workbuf);

	snprintf(workbuf, sizeof workbuf, "%s%clog", prefix, RB_PATH_SEPARATOR);
	paths[IRCD_PATH_LOG] = rb_strdup(workbuf);

	snprintf(workbuf, sizeof workbuf, "%s%chelp%cusers", prefix, RB_PATH_SEPARATOR, RB_PATH_SEPARATOR);
	paths[IRCD_PATH_USERHELP] = rb_strdup(workbuf);

	snprintf(workbuf, sizeof workbuf, "%s%chelp%copers", prefix, RB_PATH_SEPARATOR, RB_PATH_SEPARATOR);
	paths[IRCD_PATH_OPERHELP] = rb_strdup(workbuf);

	snprintf(workbuf, sizeof workbuf, "%s%cetc%circd.conf", prefix, RB_PATH_SEPARATOR, RB_PATH_SEPARATOR);
	paths[IRCD_PATH_IRCD_CONF] = rb_strdup(workbuf);

	snprintf(workbuf, sizeof workbuf, "%s%cetc%circd.motd", prefix, RB_PATH_SEPARATOR, RB_PATH_SEPARATOR);
	paths[IRCD_PATH_IRCD_MOTD] = rb_strdup(workbuf);

	snprintf(workbuf, sizeof workbuf, "%s%cetc%copers.motd", prefix, RB_PATH_SEPARATOR, RB_PATH_SEPARATOR);
	paths[IRCD_PATH_IRCD_OMOTD] = rb_strdup(workbuf);

	snprintf(workbuf, sizeof workbuf, "%s%cetc%cban.db", prefix, RB_PATH_SEPARATOR, RB_PATH_SEPARATOR);
	paths[IRCD_PATH_BANDB] = rb_strdup(workbuf);

	snprintf(workbuf, sizeof workbuf, "%s%cetc%circd.pid", prefix, RB_PATH_SEPARATOR, RB_PATH_SEPARATOR);
	paths[IRCD_PATH_IRCD_PID] = rb_strdup(workbuf);

	snprintf(workbuf, sizeof workbuf, "%s%clogs%circd.log", prefix, RB_PATH_SEPARATOR, RB_PATH_SEPARATOR);
	paths[IRCD_PATH_IRCD_LOG] = rb_strdup(workbuf);

	snprintf(workbuf, sizeof workbuf, "%s%cbin", prefix, RB_PATH_SEPARATOR);
	paths[IRCD_PATH_BIN] = rb_strdup(workbuf);
	paths[IRCD_PATH_LIBEXEC] = rb_strdup(workbuf);

	inotice("runtime paths:");
	for (int i = 0; i < IRCD_PATH_COUNT; i++)
	{
		inotice("  %s: %s", pathnames[i], paths[i]);
	}
}
#else
void fs::relocate_paths()
{
}
#endif
