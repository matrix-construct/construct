// Matrix Construct
//
// Copyright (C) Matrix Construct Developers, Authors & Contributors
// Copyright (C) 2016-2018 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

#include <RB_INC_UNISTD_H
#include <RB_INC_CPUID_H
#include <RB_INC_SYS_SYSINFO_H
#include <RB_INC_GNU_LIBC_VERSION_H

namespace ircd::info
{
	static void dump_exe_info();
	static void dump_lib_info();
	static void dump_sys_info();
	static void dump_cpu_info();
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

void
ircd::info::dump()
{
	dump_exe_info();
	dump_lib_info();
	dump_sys_info();
	dump_cpu_info();
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

// Required for instance_list template instantiation.
ircd::info::versions::~versions()
noexcept
{
}

///////////////////////////////////////////////////////////////////////////////
//
// Hardware / Platform
//

void
ircd::info::dump_cpu_info()
{
	// This message flashes hardware standard information about this platform
	#if defined(__i386__) or defined(__x86_64__)
	log::info
	{
		log::star, "%s mmx:%b sse:%b sse2:%b sse3:%b ssse3:%b sse4.1:%b sse4.2:%b avx:%b avx2:%b",
		hardware::x86::vendor,
		hardware::x86::mmx,
		hardware::x86::sse,
		hardware::x86::sse2,
		hardware::x86::sse3,
		hardware::x86::ssse3,
		hardware::x86::sse4_1,
		hardware::x86::sse4_2,
		hardware::x86::avx,
		hardware::x86::avx2,
	};
	#endif

	// This message flashes language standard information about this platform
	#ifdef RB_DEBUG
	log::logf
	{
		log::star, log::DEBUG,
		"max_align=%zu hw_conc=%zu d_inter=%zu c_inter=%zu",
		hardware::max_align,
		hardware::hardware_concurrency,
		hardware::destructive_interference,
		hardware::constructive_interference,
	};
	#endif

	if(!ircd::debugmode)
		return;

	log::debug
	{
		log::star,
		"0..00 STD MANUFAC [%08x|%08x|%08x|%08x] "
		"0..01 STD FEATURE [%08x|%08x|%08x|%08x]",
		uint32_t(hardware::x86::manufact >> 0),
		uint32_t(hardware::x86::manufact >> 32),
		uint32_t(hardware::x86::manufact >> 64),
		uint32_t(hardware::x86::manufact >> 96),
		uint32_t(hardware::x86::features >> 0),
		uint32_t(hardware::x86::features >> 32),
		uint32_t(hardware::x86::features >> 64),
		uint32_t(hardware::x86::features >> 96),
	};

	log::debug
	{
		log::star,
		"8..00 EXT MANUFAC [%08x|%08x|%08x|%08x] "
		"8..01 EXT FEATURE [%08x|%08x|%08x|%08x]",
		uint32_t(hardware::x86::_manufact >> 0),
		uint32_t(hardware::x86::_manufact >> 32),
		uint32_t(hardware::x86::_manufact >> 64),
		uint32_t(hardware::x86::_manufact >> 96),
		uint32_t(hardware::x86::_features >> 0),
		uint32_t(hardware::x86::_features >> 32),
		uint32_t(hardware::x86::_features >> 64),
		uint32_t(hardware::x86::_features >> 96),
	};

	log::debug
	{
		log::star,
		"8..07 EXT APMI    [%08x|%08x|%08x|%08x] "
		"8..1C EXT LWPROF  [%08x|%08x|%08x|%08x]",
		uint32_t(hardware::x86::_apmi >> 0),
		uint32_t(hardware::x86::_apmi >> 32),
		uint32_t(hardware::x86::_apmi >> 64),
		uint32_t(hardware::x86::_apmi >> 96),
		uint32_t(hardware::x86::_lwp >> 0),
		uint32_t(hardware::x86::_lwp >> 32),
		uint32_t(hardware::x86::_lwp >> 64),
		uint32_t(hardware::x86::_lwp >> 96),
	};
}

decltype(ircd::info::hardware::max_align)
ircd::info::hardware::max_align
{
	alignof(std::max_align_t)
};

decltype(ircd::info::hardware::hardware_concurrency)
ircd::info::hardware::hardware_concurrency
{
	std::thread::hardware_concurrency()
};

decltype(ircd::info::hardware::destructive_interference)
ircd::info::hardware::destructive_interference
{
	#ifdef __cpp_lib_hardware_interference_size
		std::hardware_destructive_interference_size
	#else
		0
	#endif
};

decltype(ircd::info::hardware::constructive_interference)
ircd::info::hardware::constructive_interference
{
	#ifdef __cpp_lib_hardware_interference_size
		std::hardware_constructive_interference_size
	#else
		0
	#endif
};

//
// x86::x86
//

decltype(ircd::info::hardware::x86::manufact)
ircd::info::hardware::x86::manufact
{
	cpuid(0x00000000U, 0),
};

decltype(ircd::info::hardware::x86::features)
ircd::info::hardware::x86::features
{
	cpuid(0x00000001U, 0)
};

decltype(ircd::info::hardware::x86::extended_features)
ircd::info::hardware::x86::extended_features
{
	cpuid(0x00000007U, 0)
};

decltype(ircd::info::hardware::x86::_manufact)
ircd::info::hardware::x86::_manufact
{
	cpuid(0x80000000U, 0),
};

decltype(ircd::info::hardware::x86::_features)
ircd::info::hardware::x86::_features
{
	cpuid(0x80000001U, 0)
};

decltype(ircd::info::hardware::x86::_apmi)
ircd::info::hardware::x86::_apmi
{
	cpuid(0x80000007U, 0)
};

decltype(ircd::info::hardware::x86::_lwp)
ircd::info::hardware::x86::_lwp
{
	cpuid(0x8000001CU, 0)
};

static char
ircd_info_hardware_x86_vendor[16];

decltype(ircd::info::hardware::x86::vendor)
ircd::info::hardware::x86::vendor{[]
{
	char *const out{ircd_info_hardware_x86_vendor};
	const auto in{reinterpret_cast<const uint8_t *>(&manufact)};

	out[0] = in[4];  out[1] = in[5];  out[2] = in[6];   out[3] = in[7];
	out[4] = in[12]; out[5] = in[13]; out[6] = in[14];  out[7] = in[15];
	out[8] = in[8];  out[9] = in[9];  out[10] = in[10]; out[11] = in[11];

	return string_view
	{
		ircd_info_hardware_x86_vendor
	};
}()};

decltype(ircd::info::hardware::x86::mmx)
ircd::info::hardware::x86::mmx
{
	bool(features & (uint128_t(1) << (96 + 23)))
};

decltype(ircd::info::hardware::x86::sse)
ircd::info::hardware::x86::sse
{
	bool(features & (uint128_t(1) << (96 + 26)))
};

decltype(ircd::info::hardware::x86::sse2)
ircd::info::hardware::x86::sse2
{
	bool(features & (uint128_t(1) << (96 + 0)))
};

decltype(ircd::info::hardware::x86::sse3)
ircd::info::hardware::x86::sse3
{
	bool(features & (uint128_t(1) << (64 + 0)))
};

decltype(ircd::info::hardware::x86::ssse3)
ircd::info::hardware::x86::ssse3
{
	bool(features & (uint128_t(1) << (64 + 9)))
};

decltype(ircd::info::hardware::x86::sse4_1)
ircd::info::hardware::x86::sse4_1
{
	bool(features & (uint128_t(1) << (64 + 19)))
};

decltype(ircd::info::hardware::x86::sse4_2)
ircd::info::hardware::x86::sse4_2
{
	bool(features & (uint128_t(1) << (64 + 20)))
};

decltype(ircd::info::hardware::x86::avx)
ircd::info::hardware::x86::avx
{
	bool(features & (uint128_t(1) << (64 + 28)))
};

decltype(ircd::info::hardware::x86::avx2)
ircd::info::hardware::x86::avx2
{
	bool(features & (uint128_t(1) << (32 + 5)))
};

#ifdef __x86_64__
ircd::uint128_t
ircd::info::hardware::x86::cpuid(const uint &leaf,
                                 const uint &subleaf)
{
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
ircd::uint128_t
ircd::info::hardware::x86::cpuid(const uint &leaf,
                                 const uint &subleaf)
{
	return 0;
}
#endif

///////////////////////////////////////////////////////////////////////////////
//
// System information
//

void
ircd::info::dump_sys_info()
{
	// This message flashes posix information about the system and platform IRCd
	// is running on when ::uname() is available
	#ifdef HAVE_SYS_UTSNAME_H
	log::info
	{
		log::star, "%s %s %s %s %s",
		utsname.sysname,
		utsname.nodename,
		utsname.release,
		utsname.version,
		utsname.machine
	};
	#endif

	// Dump detected filesystem related to log.
	fs::support::dump_info();

	// Additional detected system parameters
	#ifdef RB_DEBUG
	char buf[2][48];
	log::logf
	{
		log::star, log::DEBUG,
		"page_size=%zu iov_max=%zu aio_max=%zu aio_reqprio_max=%zu ram=%s swap=%s",
		page_size,
		iov_max,
		aio_max,
		aio_reqprio_max,
		pretty(buf[0], iec(total_ram)),
		pretty(buf[1], iec(total_swap)),
	};
	#endif
}

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
			return lex_castable<int>(str)?
				lex_cast<int>(str):
				0;
		}(),

		[] // minor
		{
			auto str(split(kernel_release, '.').second);
			str = split(str, '.').first;
			return lex_castable<int>(str)?
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
// System configuration
//

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
// System information
//

namespace ircd::info
{
	#ifdef HAVE_SYS_SYSINFO_H
	extern struct ::sysinfo sysinfo;
	#endif
}

#ifdef HAVE_SYS_SYSINFO_H
decltype(ircd::info::sysinfo)
ircd::info::sysinfo{[]
{
	struct ::sysinfo ret;
	syscall(::sysinfo, &ret);
	return ret;
}()};
#endif

decltype(ircd::info::total_ram)
ircd::info::total_ram
{
	#ifdef HAVE_SYS_SYSINFO_H
	sysinfo.totalram
	#endif
};

decltype(ircd::info::total_swap)
ircd::info::total_swap
{
	#ifdef HAVE_SYS_SYSINFO_H
	sysinfo.totalswap
	#endif
};

///////////////////////////////////////////////////////////////////////////////
//
// Userspace / Library
//

void
ircd::info::dump_lib_info()
{
	// This message flashes information about our API dependencies from compile time.
	log::info
	{
		log::star, "%s SD-6 %s. glibcxx %s. glibc %s. boost %s. RocksDB %s. sodium %s. %s. magic %ld.",
		string_view{RB_CXX},
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
		log::star, "Linked: glibc %s. boost %s. RocksDB %s. sodium %s. %s. magic %ld.",
		string_view{glibc_version_abi},
		string_view{boost_version_abi},
		string_view{db::version_abi},
		string_view{nacl::version_abi},
		string_view{openssl::version_abi},
		long(magic::version_abi),
	};
}

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

///////////////////////////////////////////////////////////////////////////////
//
// Primary / Executable
//

void
ircd::info::dump_exe_info()
{
	// This message flashes information about IRCd itself for this execution.
	log::info
	{
		log::star, "%s %s configured: %s; compiled: %s; executed: %s; %s",
		BRANDING_NAME,
		BRANDING_VERSION,
		configured,
		compiled,
		startup,
		RB_DEBUG_LEVEL? "(DEBUG MODE)" : ""
	};
}

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

decltype(ircd::info::startup_time)
ircd::info::startup_time
{
	std::time(nullptr)
};

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

decltype(ircd::info::configured_time)
ircd::info::configured_time
{
	RB_TIME_CONFIGURED
};

decltype(ircd::info::configured)
ircd::info::configured
{
	rstrip(ctime(&configured_time), '\n')
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
