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
#include <RB_INC_UNISTD_H

void
ircd::info::init()
{

}

void
ircd::info::dump()
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
		"%s. glibc %s. boost %s. RocksDB %s. SpiderMonkey %s. sodium %s. %s. libmagic %d.",
		PACKAGE_STRING,
		glibc_version_str,
		boost_version_str,
		db::version_str,
		js::version(js::ver::IMPLEMENTATION),
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
		"page_size=%zu max_align=%zu hw_conc=%zu d_inter=%zu c_inter=%zu",
		page_size,
		max_align,
		hardware_concurrency,
		destructive_interference,
		constructive_interference,
	};

	// This message flashes posix information about the resource limits
	log::debug
	{
		"AS=%lu DATA=%lu RSS=%lu NOFILE=%lu; RTTIME=%lu iov_max=%zu aio_max=%zu aio_reqprio_max=%zu",
		rlimit_as,
		rlimit_data,
		rlimit_rss,
		rlimit_nofile,
		rlimit_rttime,
		iov_max,
		aio_max,
		aio_reqprio_max,
	};
}

decltype(ircd::info::credits)
ircd::info::credits
{
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
	nullptr
};

//
// Primary information
//

decltype(ircd::info::server_agent)
ircd::info::server_agent
{
	BRANDING_NAME " (IRCd " BRANDING_VERSION ")"
};

decltype(ircd::info::user_agent)
ircd::info::user_agent
{
	BRANDING_NAME " (IRCd " BRANDING_VERSION ")"
};

decltype(ircd::info::version)
ircd::info::version
{
	RB_VERSION
};

decltype(ircd::info::name)
ircd::info::name
{
	PACKAGE_NAME
};

extern "C" const char *const
ircd_version
{
	RB_VERSION
};

extern "C" const char *const
ircd_name
{
	PACKAGE_NAME
};

//
// Third party dependency information
//

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

decltype(ircd::info::glibc_version)
ircd::info::glibc_version
{
	__GNU_LIBRARY__,
	__GLIBC__,
	__GLIBC_MINOR__,
};

char ircd_info_glibc_version_str_buf[32];
decltype(ircd::info::glibc_version_str)
ircd::info::glibc_version_str
(
	ircd_info_glibc_version_str_buf,
    ::snprintf(ircd_info_glibc_version_str_buf, sizeof(ircd_info_glibc_version_str_buf),
               "%d.%d.%d",
               glibc_version[0],
               glibc_version[1],
               glibc_version[2])
);

//
// Host information
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

//
// System information
//

#ifdef HAVE_SYS_RESOURCE_H
static uint64_t
_get_rlimit(const int &resource)
{
	rlimit rlim;
	ircd::syscall(getrlimit, resource, &rlim);
	return rlim.rlim_cur;
}
#else
static uint64_t
_get_rlimit(const int &resource)
{
	return 0;
}
#endif

decltype(ircd::info::rlimit_rttime)
ircd::info::rlimit_rttime
{
	#ifdef RLIMIT_RTTIME
	_get_rlimit(RLIMIT_RTTIME)
	#endif
};

decltype(ircd::info::rlimit_nofile)
ircd::info::rlimit_nofile
{
	#ifdef RLIMIT_NOFILE
	_get_rlimit(RLIMIT_NOFILE)
	#endif
};

decltype(ircd::info::rlimit_rss)
ircd::info::rlimit_rss
{
	#ifdef RLIMIT_RSS
	_get_rlimit(RLIMIT_RSS)
	#endif
};

decltype(ircd::info::rlimit_data)
ircd::info::rlimit_data
{
	#ifdef RLIMIT_DATA
	_get_rlimit(RLIMIT_DATA)
	#endif
};

decltype(ircd::info::rlimit_as)
ircd::info::rlimit_as
{
	#ifdef RLIMIT_AS
	_get_rlimit(RLIMIT_AS)
	#endif
};

decltype(ircd::info::aio_reqprio_max)
ircd::info::aio_reqprio_max
{
	#ifdef _SC_AIO_PRIO_DELTA_MAX
	size_t(syscall(::sysconf, _SC_AIO_PRIO_DELTA_MAX))
	#endif
};

decltype(ircd::info::aio_max)
ircd::info::aio_max
{
	#ifdef _SC_AIO_MAX
	0 //size_t(syscall(::sysconf, _SC_AIO_MAX))
	#endif
};

decltype(ircd::info::iov_max)
ircd::info::iov_max
{
	#ifdef _SC_IOV_MAX
	size_t(syscall(::sysconf, _SC_IOV_MAX))
	#endif
};

decltype(ircd::info::page_size)
ircd::info::page_size
{
	#ifdef _SC_PAGESIZE
	size_t(syscall(::sysconf, _SC_PAGESIZE))
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

decltype(ircd::info::destructive_interference)
ircd::info::destructive_interference
{
	#ifdef __cpp_lib_hardware_interference_size
		std::hardware_destructive_interference_size
	#else
		0
	#endif
};

decltype(ircd::info::hardware_concurrency)
ircd::info::hardware_concurrency
{
	std::thread::hardware_concurrency()
};

decltype(ircd::info::max_align)
ircd::info::max_align
{
	alignof(std::max_align_t)
};

//
// Build information
//

decltype(ircd::info::startup)
ircd::info::startup
{
	ctime(&startup_time)
};

decltype(ircd::info::compiled)
ircd::info::compiled
{
	__TIMESTAMP__
};

decltype(ircd::info::configured)
ircd::info::configured
{
	ctime(&configured_time)
};

decltype(ircd::info::startup_time)
ircd::info::startup_time
{
	std::time(nullptr)
};

decltype(ircd::info::configured_time)
ircd::info::configured_time
{
	RB_TIME_CONFIGURED
};

decltype(ircd::info::commit)
ircd::info::commit
{
	RB_VERSION_COMMIT
};

decltype(ircd::info::branch)
ircd::info::branch
{
	RB_VERSION_BRANCH
};

decltype(ircd::info::tag)
ircd::info::tag
{
	RB_VERSION_TAG
};
