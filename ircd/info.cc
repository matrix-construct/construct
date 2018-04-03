// Matrix Construct
//
// Copyright (C) Matrix Construct Developers, Authors & Contributors
// Copyright (C) 2016-2018 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

#include <RB_INC_SYS_RESOURCE_H
#include <ircd/asio.h>

void
ircd::info::init()
{
	// This message flashes information about IRCd itself for this execution.
	log::info
	{
		"%s %ld %s. configured: %s; compiled: %s; executed: %s; %s",
		BRANDING_VERSION,
		__cplusplus,
		__VERSION__,
		configured,
		compiled,
		startup,
		RB_DEBUG_LEVEL? "(DEBUG MODE)" : ""
	};

	// This message flashes information about our dependencies which are being
	// assumed for this execution.
	log::info
	{
		"%s. boost %u.%u.%u. rocksdb %s. sodium %s. %s. libmagic %d.",
		PACKAGE_STRING,
		boost_version[0],
		boost_version[1],
		boost_version[2],
		db::version,
		nacl::version(),
		openssl::version(),
		magic::version()
	};

	// This message flashes posix information about the system and platform IRCd
	// is running on when ::uname() is available
	#ifdef HAVE_SYS_UTSNAME_H
	log::info
	{
		"%s %s %s %s %s",
		utsname.sysname,
		utsname.nodename,
		utsname.release,
		utsname.version,
		utsname.machine
	};
	#endif

	// This message flashes standard information about the system and platform
	// IRCd is compiled for and running on.
	log::debug
	{
		"max_align=%zu hw_conc=%zu d_inter=%zu c_inter=%zu",
		max_align,
		hardware_concurrency,
		destructive_interference,
		constructive_interference
	};
}

extern "C" const char *const
ircd_name
{
	PACKAGE_NAME
};

extern "C" const char *const
ircd_version
{
	RB_VERSION
};

decltype(ircd::info::name)
ircd::info::name
{
	PACKAGE_NAME
};

decltype(ircd::info::version)
ircd::info::version
{
	RB_VERSION
};

decltype(ircd::info::user_agent)
ircd::info::user_agent
{
	BRANDING_NAME " (IRCd " BRANDING_VERSION ")"
};

decltype(ircd::info::server_agent)
ircd::info::server_agent
{
	BRANDING_NAME " (IRCd " BRANDING_VERSION ")"
};

//
// IRCd / build information
//

decltype(ircd::info::tag)
ircd::info::tag
{
	RB_VERSION_TAG
};

decltype(ircd::info::branch)
ircd::info::branch
{
	RB_VERSION_BRANCH
};

decltype(ircd::info::commit)
ircd::info::commit
{
	RB_VERSION_COMMIT
};

decltype(ircd::info::configured_time)
ircd::info::configured_time
{
	RB_TIME_CONFIGURED
};

decltype(ircd::info::compiled_time)
ircd::info::compiled_time
{
	RB_TIME_COMPILED
};

decltype(ircd::info::startup_time)
ircd::info::startup_time
{
	std::time(nullptr)
};

decltype(ircd::info::configured)
ircd::info::configured
{
	ctime(&configured_time)
};

decltype(ircd::info::compiled)
ircd::info::compiled
{
	//ctime(&compiled_time)
	__TIMESTAMP__
};

decltype(ircd::info::startup)
ircd::info::startup
{
	ctime(&startup_time)
};

//
// System / platform information
//

#ifdef HAVE_SYS_UTSNAME_H
decltype(ircd::info::utsname)
ircd::info::utsname{[]
{
	struct ::utsname utsname;
	syscall(::uname, &utsname);
	return utsname;
}()};
#endif

#ifdef HAVE_SYS_RESOURCE_H
static uint64_t
_get_rlimit(const int &resource)
{
	rlimit rlim;
	ircd::syscall(getrlimit, RLIMIT_AS, &rlim);
	return rlim.rlim_cur;
}
#else
static uint64_t
_get_rlimit(const int &resource)
{
	return 0;
}
#endif

decltype(ircd::info::rlimit_as)
ircd::info::rlimit_as
{
	_get_rlimit(RLIMIT_AS)
};

decltype(ircd::info::rlimit_data)
ircd::info::rlimit_data
{
	_get_rlimit(RLIMIT_DATA)
};

decltype(ircd::info::rlimit_rss)
ircd::info::rlimit_rss
{
	_get_rlimit(RLIMIT_RSS)
};

decltype(ircd::info::max_align)
ircd::info::max_align
{
	alignof(std::max_align_t)
};

decltype(ircd::info::hardware_concurrency)
ircd::info::hardware_concurrency
{
	std::thread::hardware_concurrency()
};

decltype(ircd::info::destructive_interference)
ircd::info::destructive_interference
{
	#ifdef __cpp_lib_hardware_interference_size
		std::hardware_destructive_interference_size
	#else
		0
	#endif
};

decltype(ircd::info::constructive_interference)
ircd::info::constructive_interference
{
	#ifdef __cpp_lib_hardware_interference_size
		std::hardware_constructive_interference_size
	#else
		0
	#endif
};

//
// Third party dependency information
//

/// Boost version indicator for compiled header files.
decltype(ircd::info::boost_version)
ircd::info::boost_version
{
	BOOST_VERSION / 100000,
	BOOST_VERSION / 100 % 1000,
	BOOST_VERSION % 100,
};

/// Provides tcmalloc version information if tcmalloc is linked in to IRCd.
struct ircd::info::tc_version
{
	int major{0}, minor{0};
	char patch[64] {0};
	std::string version {"unavailable"};
}
const ircd::info::tc_version;

/*
const char* tc_version(int* major, int* minor, const char** patch);
ircd::tc_version::tc_version()
:version{::tc_version(&major, &minor, reinterpret_cast<const char **>(&patch))}
{}
*/

//
// Extended information
//

//TODO: XXX: integrate CREDITS text again somehow
decltype(ircd::info::credits)
ircd::info::credits
{{

	"Inspired by the original Internet Relay Chat daemon from Jarkko Oikarinen",
	" ",
	"This - is The Construct",
	" ",
	"Internet Relay Chat daemon: Matrix Construct",
	" ",
	"Copyright (C) 2016-2018 Matrix Construct Developers, Authors & Contributors",
	"Permission to use, copy, modify, and/or distribute this software for any",
	"purpose with or without fee is hereby granted, provided that the above",
	"copyright notice and this permission notice is present in all copies.",
	" ",
}};

decltype(ircd::info::myinfo)
ircd::info::myinfo
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
