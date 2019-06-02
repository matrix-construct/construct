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
#include <RB_INC_CPUID_H
#include <RB_INC_GNU_LIBC_VERSION_H

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

void
ircd::info::dump()
{
	// This message flashes information about IRCd itself for this execution.
	log::info
	{
		"%s configured: %s; compiled: %s; executed: %s; %s",
		BRANDING_VERSION,
		configured,
		compiled,
		startup,
		RB_DEBUG_LEVEL? "(DEBUG MODE)" : ""
	};

	// This message flashes information about our API dependencies from compile time.
	log::info
	{
		"SD-6 %s. glibcxx %s. glibc %s. boost %s. RocksDB %s. sodium %s. %s. magic %ld.",
		string_view{sd6_version},
		string_view{glibcxx_version_api},
		string_view{glibc_version_api},
		string_view{boost_version_api},
		string_view{db::version_api},
		string_view{nacl::version_api},
		string_view{openssl::version_api},
		long(magic::version_api),
	};

	// This message flashes information about our ABI dependencies on this system.
	log::info
	{
		"Linked: glibc %s. boost %s. RocksDB %s. sodium %s. %s. magic %ld.",
		string_view{glibc_version_abi},
		string_view{boost_version_abi},
		string_view{db::version_abi},
		string_view{nacl::version_abi},
		string_view{openssl::version_abi},
		long(magic::version_abi),
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

	// This message flashes posix information about the resource limits
	log::debug
	{
		"AS=%lu DATA=%lu RSS=%lu NOFILE=%lu; RTTIME=%lu"
		" page_size=%zu iov_max=%zu aio_max=%zu aio_reqprio_max=%zu",
		rlimit_as,
		rlimit_data,
		rlimit_rss,
		rlimit_nofile,
		rlimit_rttime,
		page_size,
		iov_max,
		aio_max,
		aio_reqprio_max,
	};

	// This message flashes standard information about the system and platform
	// IRCd is compiled for and running on.
	log::debug
	{
		"cpu[%s] max_align=%zu hw_conc=%zu d_inter=%zu c_inter=%zu",
		cpuvendor,
		max_align,
		hardware_concurrency,
		destructive_interference,
		constructive_interference,
	};

	// This message flashes standard hardware feature information
	#ifdef __x86_64__
	log::debug
	{
		"0..01 [%08x|%08x|%08x|%08x] "
		"0..07 [%08x|%08x|%08x|%08x]",
		uint32_t(cpuid[1]),
		uint32_t(cpuid[1] >> 32),
		uint32_t(cpuid[1] >> 64),
		uint32_t(cpuid[1] >> 96),
		uint32_t(cpuid[3]),
		uint32_t(cpuid[3] >> 32),
		uint32_t(cpuid[3] >> 64),
		uint32_t(cpuid[3] >> 96),
	};
	#endif

	#ifdef __x86_64__
	log::debug
	{
		"8..01 [%08x|%08x|%08x|%08x] "
		"8..1c [%08x|%08x|%08x|%08x]",
		uint32_t(cpuid[5]),
		uint32_t(cpuid[5] >> 32),
		uint32_t(cpuid[5] >> 64),
		uint32_t(cpuid[5] >> 96),
		uint32_t(cpuid[6]),
		uint32_t(cpuid[6] >> 32),
		uint32_t(cpuid[6] >> 64),
		uint32_t(cpuid[6] >> 96),
	};
	#endif
}

//
// Version registry
//

template<>
decltype(ircd::util::instance_list<ircd::info::versions>::allocator)
ircd::util::instance_list<ircd::info::versions>::allocator
{};

template<>
decltype(ircd::util::instance_list<ircd::info::versions>::list)
ircd::util::instance_list<ircd::info::versions>::list
{
    allocator
};

/// Straightforward construction of versions members; string is copied
/// into member buffer with null termination.
ircd::info::versions::versions(const string_view &name,
                               const enum type &type,
                               const long &monotonic,
                               const std::array<long, 3> &semantic,
                               const string_view &string)
:versions
{
	name, type, monotonic, semantic, [&string]
	(auto &that, const mutable_buffer &buf)
	{
		strlcpy(buf, string);
	}
}
{
}

/// Construction of versions members with closure for custom string generation.
/// The version string must be stored into buffer with null termination.
ircd::info::versions::versions(const string_view &name,
                               const enum type &type,
                               const long &monotonic,
                               const std::array<long, 3> &semantic,
                               const std::function<void (versions &, const mutable_buffer &)> &closure)
:name{name}
,type{type}
,monotonic{monotonic}
,semantic{semantic}
,string{'\0'}
{
	closure(*this, this->string);

	// User provided a string already; nothing else to do
	if(strnlen(this->string, sizeof(this->string)) != 0)
		return;

	// Generate a string from the semantic version number or if all zeroes
	// from the monotonic version number instead.
	if(!this->semantic[0] && !this->semantic[1] && !this->semantic[2])
		::snprintf(this->string, sizeof(this->string), "%ld",
		           this->monotonic);
	else
		::snprintf(this->string, sizeof(this->string), "%ld.%ld.%ld",
		           this->semantic[0],
		           this->semantic[1],
		           this->semantic[2]);
}

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

//
// System information
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
// kernel
//

decltype(ircd::info::kernel_name)
ircd::info::kernel_name
{
	#ifdef HAVE_SYS_UTSNAME_H
	utsname.sysname
	#endif
};

decltype(ircd::info::kernel_release)
ircd::info::kernel_release
{
	#ifdef HAVE_SYS_UTSNAME_H
	utsname.release
	#endif
};

decltype(ircd::info::kernel_version)
ircd::info::kernel_version
{
	"kernel", versions::ABI, 0,
	{
		[] // major
		{
			const auto str(split(kernel_release, '.').first);
			return try_lex_cast<int>(str)?
				lex_cast<int>(str):
				0;
		}(),

		[] // minor
		{
			auto str(split(kernel_release, '.').second);
			str = split(str, '.').first;
			return try_lex_cast<int>(str)?
				lex_cast<int>(str):
				0;
		}(),

		0 // patch
	},

	[](auto &that, const auto &buf)
	{
		::snprintf(data(buf), size(buf), "%s %s",
		           utsname.sysname,
		           utsname.release);
	}
};

//
// gnuc
//

decltype(ircd::info::gnuc_version)
ircd::info::gnuc_version
{
	"gnuc", versions::API, 0,
	{
		#if defined(__GNUC__)
			__GNUC__,
		#endif

		#if defined(__GNUC_MINOR__)
			__GNUC_MINOR__,
		#endif

		#if defined(__GNUC_PATCHLEVEL__)
			__GNUC_PATCHLEVEL__,
		#endif
	},

	#if defined(__VERSION__)
		__VERSION__
	#endif
};

//
// clang
//

decltype(ircd::info::clang_version)
ircd::info::clang_version
{
	"clang", versions::API, 0,
	{
		#if defined(__clang_major__)
			__clang_major__,
		#endif

		#if defined(__clang_minor__)
			__clang_minor__,
		#endif

		#if defined(__clang_patchlevel__)
			__clang_patchlevel__,
		#endif
	},

	#if defined(__clang_version__)
		__clang_version__
	#endif
};

//
// glibc
//

decltype(ircd::info::glibc_version_api)
ircd::info::glibc_version_api
{
	"glibc", versions::API, 0,
	{
		#if defined(__GNU_LIBRARY__)
			__GNU_LIBRARY__,
		#endif

		#if defined(__GLIBC__)
			__GLIBC__,
		#endif

		#if defined(__GLIBC_MINOR__)
			__GLIBC_MINOR__,
		#endif
	},
};

decltype(ircd::info::glibc_version_abi)
ircd::info::glibc_version_abi
{
	"glibc", versions::ABI, 0, {0},

	#if defined(HAVE_GNU_LIBC_VERSION_H)
		::gnu_get_libc_version()
	#endif
};

//
// glibcxx
//

decltype(ircd::info::glibcxx_version_api)
ircd::info::glibcxx_version_api
{
	"glibcxx", versions::API,

	#if defined(__GLIBCXX__)
		__GLIBCXX__
	#endif
};

//
// sd6
//

decltype(ircd::info::sd6_version)
ircd::info::sd6_version
{
	"SD-6", versions::API, __cplusplus
};

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

#ifdef _SC_CLK_TCK
decltype(ircd::info::clk_tck)
ircd::info::clk_tck
{
	size_t(syscall(::sysconf, _SC_CLK_TCK))
};
#else
decltype(ircd::info::clk_tck)
ircd::info::clk_tck
{
	1 // prevent #DE
};
#endif

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

//
// Platform information
//

#ifdef __x86_64__
static ircd::uint128_t
get_cpuid(const uint &leaf,
          const uint &subleaf)
{
	using ircd::uint128_t;

	uint32_t reg[4];
	asm volatile
	(
		"cpuid" "\n\t"
		: "=a"  (reg[0]),
		  "=b"  (reg[1]),
		  "=c"  (reg[2]),
		  "=d"  (reg[3])
		: "0"  (leaf),
		  "2"  (subleaf)
	);

	return uint128_t
	{
		(uint128_t(reg[3]) << 96) |  // edx
		(uint128_t(reg[2]) << 64) |  // ecx
		(uint128_t(reg[1]) << 32) |  // ebx
		(uint128_t(reg[0]) << 0)     // eax
	};
}
#else
static ircd::uint128_t
get_cpuid(const uint &leaf,
          const uint &subleaf)
{
	return 0;
}
#endif

decltype(ircd::info::cpuid)
ircd::info::cpuid
{
	get_cpuid(0x00000000U, 0),
	get_cpuid(0x00000001U, 0),
	0UL,
	get_cpuid(0x00000007U, 0U),
	get_cpuid(0x80000000U, 0),
	get_cpuid(0x80000001U, 0),
	get_cpuid(0x8000001CU, 0), //AMD Vol.2 13.4.3.3 (LWP)
	0UL,
};

char
_cpuvendor_[12];

decltype(ircd::info::cpuvendor)
ircd::info::cpuvendor{[&]
{
	const auto b
	{
		reinterpret_cast<const uint8_t *>(cpuid + 0)
	};

	_cpuvendor_[0] = b[4];
	_cpuvendor_[1] = b[5];
	_cpuvendor_[2] = b[6];
	_cpuvendor_[3] = b[7];
	_cpuvendor_[4] = b[12];
	_cpuvendor_[5] = b[13];
	_cpuvendor_[6] = b[14];
	_cpuvendor_[7] = b[15];
	_cpuvendor_[8] = b[8];
	_cpuvendor_[9] = b[9];
	_cpuvendor_[10] = b[10];
	_cpuvendor_[11] = b[11];

	return string_view
	{
		_cpuvendor_, sizeof(_cpuvendor_)
	};
}()};

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
	rstrip(ctime(&startup_time), '\n')
};

decltype(ircd::info::compiled)
ircd::info::compiled
{
	__TIMESTAMP__
};

decltype(ircd::info::configured)
ircd::info::configured
{
	rstrip(ctime(&configured_time), '\n')
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
