/*
 *   Copyright (C) 1990 Chelsea Ashley Dyerman
 *   Copyright (C) 2008 ircd-ratbox development team
 *   Copyright (C) 2016 Charybdis Development Team
 *   Copyright (C) 2016 Jason Volk <jason@zemos.net>
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 2, or (at your option)
 *   any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program; if not, write to the Free Software
 *   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

using namespace ircd;

const time_t
info::configured_time
{
	RB_TIME_CONFIGURED
};

const time_t
info::compiled_time
{
	RB_TIME_COMPILED
};

const time_t
info::startup_time
{
	std::time(nullptr)
};

const std::string
info::configured
{
	ctime(&configured_time)
};

const std::string
info::compiled
{
	ctime(&compiled_time)
};

const std::string
info::startup
{
	ctime(&startup_time)
};

const std::string
info::serno
{
	//TODO: XXX: compile counter?
	// RB_SERNO
	0
};

const std::string
info::version
{
	RB_VERSION
};

const char *const
info::ircd_version
{
	RB_VERSION
};

/* XXX: integrate CREDITS text again somehow */
const std::vector<std::string>
info::credits
{{
	"charybdis",
	"Based on the original code written by Jarkko Oikarinen",
	"Copyright (c) 1988-1991 University of Oulu, Computing Center",
	"Copyright (c) 1996-2001 Hybrid Development Team",
	"Copyright (c) 2002-2009 ircd-ratbox Development Team",
	"Copyright (c) 2005-2016 charybdis development team",
	" ",
	"This program is free software; you can redistribute it and/or",
	"modify it under the terms of the GNU General Public License as",
	"published by the Free Software Foundation; either version 2, or",
	"(at your option) any later version.",
	" ",
}};

const std::vector<info::line>
info::myinfo
{{
	#ifdef CPATH
	{"CPATH", CPATH, 0, "Path to Main Configuration File"},
	#else
	{"CPATH", "NONE", 0, "Path to Main Configuration File"},
	#endif

	#ifdef DPATH
	{"DPATH", DPATH, 0, "Directory Containing Configuration Files"},
	#else
	{"DPATH", "NONE", 0, "Directory Containing Configuration Files"},
	#endif

	#ifdef HPATH
	{"HPATH", HPATH, 0, "Path to Operator Help Files"},
	#else
	{"HPATH", "NONE", 0, "Path to Operator Help Files"},
	#endif

	#ifdef UHPATH
	{"UHPATH", UHPATH, 0, "Path to User Help Files"},
	#else
	{"UHPATH", "NONE", 0, "Path to User Help Files"},
	#endif

	#ifdef RB_IPV6
	{"IPV6", "ON", 0, "IPv6 Support"},
	#else
	{"IPV6", "OFF", 0, "IPv6 Support"},
	#endif

	#ifdef JOIN_LEAVE_COUNT_EXPIRE_TIME
	{"JOIN_LEAVE_COUNT_EXPIRE_TIME", "", JOIN_LEAVE_COUNT_EXPIRE_TIME, "Anti SpamBot Parameter"},
	#endif

	#ifdef KILLCHASETIMELIMIT
	{"KILLCHASETIMELIMIT", "", KILLCHASETIMELIMIT, "Nick Change Tracker for KILL"},
	#endif

	#ifdef LPATH
	{"LPATH", LPATH, 0, "Path to Log File"},
	#else
	{"LPATH", "NONE", 0, "Path to Log File"},
	#endif

	#ifdef MAX_BUFFER
	{"MAX_BUFFER", "", MAX_BUFFER, "Maximum Buffer Connections Allowed"},
	#endif

	#ifdef MAX_JOIN_LEAVE_COUNT
	{"MAX_JOIN_LEAVE_COUNT", "", MAX_JOIN_LEAVE_COUNT, "Anti SpamBot Parameter"},
	#endif

	#ifdef MAX_JOIN_LEAVE_TIME
	{"MIN_JOIN_LEAVE_TIME", "", MIN_JOIN_LEAVE_TIME, "Anti SpamBot Parameter"},
	#endif

	#ifdef MPATH
	{"MPATH", MPATH, 0, "Path to MOTD File"},
	#else
	{"MPATH", "NONE", 0, "Path to MOTD File"},
	#endif

	#ifdef NICKNAMEHISTORYLENGTH
	{"NICKNAMEHISTORYLENGTH", "", NICKNAMEHISTORYLENGTH, "Size of WHOWAS Array"},
	#endif

	#ifdef OPATH
	{"OPATH", OPATH, 0, "Path to Operator MOTD File"},
	#else
	{"OPATH", "NONE", 0, "Path to Operator MOTD File"},
	#endif

	#ifdef OPER_SPAM_COUNTDOWN
	{"OPER_SPAM_COUNTDOWN", "", OPER_SPAM_COUNTDOWN, "Anti SpamBot Parameter"},
	#endif

	#ifdef HAVE_LIBCRYPTO
	{"HAVE_LIBCRYPTO", "ON", 0, "Enable OpenSSL CHALLENGE Support"},
	#else
	{"HAVE_LIBCRYPTO", "OFF", 0, "Enable OpenSSL CHALLENGE Support"},
	#endif

	#ifdef HAVE_LIBZ
	{"HAVE_LIBZ", "YES", 0, "zlib (ziplinks) support"},
	#else
	{"HAVE_LIBZ", "NO", 0, "zlib (ziplinks)  support"},
	#endif

	#ifdef PPATH
	{"PPATH", PPATH, 0, "Path to Pid File"},
	#else
	{"PPATH", "NONE", 0, "Path to Pid File"},
	#endif

	#ifdef SPATH
	{"SPATH", SPATH, 0, "Path to Server Executable"},
	#else
	{"SPATH", "NONE", 0, "Path to Server Executable"},
	#endif

	#ifdef TS_MAX_DELTA_DEFAULT
	{"TS_MAX_DELTA_DEFAULT", "", TS_MAX_DELTA_DEFAULT, "Maximum Allowed TS Delta from another Server"},
	#endif

	#ifdef TS_WARN_DELTA_DEFAULT
	{"TS_WARN_DELTA_DEFAULT", "", TS_WARN_DELTA_DEFAULT, "Maximum TS Delta before Sending Warning"},
	#endif

	#ifdef USE_IODEBUG_HOOKS
	{"USE_IODEBUG_HOOKS", "YES", 0, "IO Debugging support"},
	#else
	{"USE_IODEBUG_HOOKS", "NO", 0, "IO Debugging support"},
	#endif
}};
