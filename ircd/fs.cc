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

namespace fs = ircd::fs;
using namespace fs;

auto paths = []
{
	using ircd::util::num_of;
	using namespace fs::path;

	std::array<const char *, num_of<fs::path::index>()> paths;

	paths[PREFIX]             = DPATH;
	paths[MODULES]            = MODPATH;
	paths[ETC]                = ETCPATH;
	paths[LOG]                = LOGPATH;
	paths[USERHELP]           = UHPATH;
	paths[OPERHELP]           = HPATH;
	paths[IRCD_EXEC]          = SPATH;
	paths[IRCD_CONF]          = CPATH;
	paths[IRCD_MOTD]          = MPATH;
	paths[IRCD_LOG]           = LPATH;
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
	names[ETC]                = "config";
	names[LOG]                = "log";
	names[USERHELP]           = "user help";
	names[OPERHELP]           = "oper help";
	names[IRCD_EXEC]          = "ircd binary";
	names[IRCD_CONF]          = "ircd.conf";
	names[IRCD_MOTD]          = "ircd.motd";
	names[IRCD_LOG]           = "ircd.log";
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
