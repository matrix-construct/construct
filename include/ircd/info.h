// Matrix Construct
//
// Copyright (C) Matrix Construct Developers, Authors & Contributors
// Copyright (C) 2016-2018 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.


#pragma once
#define HAVE_IRCD_INFO_H

// legacy
extern "C" const char *const ircd_name;
extern "C" const char *const ircd_version;

/// Information & metadata about the library.
namespace ircd::info
{
	struct line;
	struct versions;

	// Util
	void dump();

	// Primary information
	extern const string_view name;
	extern const string_view version;
	extern const string_view user_agent;
	extern const string_view server_agent;

	// Extended information
	extern const string_view credits[];

	// Build information
	extern const string_view tag;
	extern const string_view branch;
	extern const string_view commit;
	extern const time_t configured_time;
	extern const string_view configured;
	extern const string_view compiled;
	extern const string_view compiler;

	// Toolchain and library information
	extern const versions gnuc_version;
	extern const versions clang_version;
	extern const versions glibc_version_api;
	extern const versions glibc_version_abi;
	extern const versions glibcxx_version_api;
	extern const versions sd6_version;

	// System configuration / information
	extern const size_t clk_tck;
	extern const string_view clock_source;
	extern const size_t aio_reqprio_max;
	extern const size_t aio_max;
	extern const size_t iov_max;
	extern const size_t page_size;
	extern const size_t thp_size;
	extern const string_view thp_enable;
	extern const size_t total_ram;
	extern const size_t total_swap;

	// Host & third-party information
	#ifdef HAVE_SYS_UTSNAME_H
	extern const ::utsname utsname;
	#endif

	// Kernel information
	extern const string_view kernel_name;
	extern const string_view kernel_release;
	extern const versions kernel_version;
	extern const intptr_t vsyscall_p;
	extern const intptr_t vdso_p;

	// Execution information
	extern const time_t startup_time;
	extern const string_view startup;
	extern const uint64_t random;
	extern const uint uid, euid;
	extern const uint gid, egid;
	extern const bool secure;
}

namespace ircd::info::hardware
{
	extern const string_view arch;
	extern const string_view endian;
	extern const size_t max_align;
	extern const size_t hardware_concurrency;
	extern const size_t destructive_interference;
	extern const size_t constructive_interference;
	extern const size_t inst_blksz, data_blksz, uni_blksz;
	extern const size_t l1i, l1i_line, l1i_tag, l1i_assoc;
	extern const size_t l1i_tlb, l1i_tlb_assoc;
	extern const size_t l1d, l1d_line, l1d_tag, l1d_assoc;
	extern const size_t l1d_tlb, l1d_tlb_assoc;
	extern const size_t l2, l2_line, l2_tag, l2_assoc;
	extern const size_t l2_itlb, l2_itlb_assoc;
	extern const size_t l2_dtlb, l2_dtlb_assoc;
	extern const size_t l3, l3_line, l3_tag, l3_assoc;
	extern const size_t page_size;
	extern const ulonglong cap[2];
	extern const bool virtualized;
}

namespace ircd::info::hardware::x86
{
	uint8_t llc_assoc(const uint8_t) noexcept;
	uint128_t cpuid(const uint &leaf, const uint &subleaf) noexcept;

	extern const uint128_t manufact;
	extern const uint128_t features;
	extern const uint128_t extended_features;
	extern const uint128_t _manufact;
	extern const uint128_t _features;
	extern const uint128_t _l1cache;
	extern const uint128_t _llcache;
	extern const uint128_t _apmi;
	extern const uint128_t _lwp;

	extern const string_view vendor;
	extern const bool sse, sse2, sse3, ssse3, sse4a, sse4_1, sse4_2;
	extern const bool avx, avx2, avx512f;
	extern const bool tsc, tsc_constant;
};

namespace ircd::info::hardware::arm
{
	extern const uint64_t midr;
	extern const uint64_t revidr;
	extern const uint64_t isar[1];
	extern const uint64_t mmfr[1];
	extern const uint64_t pfr[1];
	extern const uint64_t ctr;

	extern const string_view vendor;
};

/// Instances of `versions` create a dynamic version registry identifying
/// third-party dependencies throughout the project and its loaded modules.
///
/// Create a static instance of this class in a definition file or module
/// which has access to the version information of the dependency. Often there
/// can be two version identifiers for a dependency, one for headers and the
/// other for dynamically loaded shared object. In that case, create two
/// instances of this class with the same name.
///
struct ircd::info::versions
:instance_list<versions>
{
	/// Our own name for the dependency.
	string_view name;

	/// Set the type to either API or ABI to indicate where this version
	/// information has been sourced. Defaults to API.
	enum type {API, ABI} type {API};

	/// If the version number is a single (likely monotonic) integer.
	long monotonic {0};

	/// Alternative semantic version number.
	std::array<long, 3> semantic {0};

	/// Version string buffer. Copy any provided version string here.
	char string[128] {0};

	/// Convenience access to read the semantic version numbers.
	auto &operator[](const int &idx) const;

	/// Convenience access to read the monotonic integer; note that if zero
	/// the semantic major number is read instead.
	explicit operator const long &() const;

	/// Convenience access to read the string
	explicit operator string_view() const;

	versions(const string_view &name,
	         const enum type &type                = type::API,
	         const long &monotonic                = 0,
	         const std::array<long, 3> &semantic  = {0L},
	         const string_view &string            = {});

	versions(const string_view &name,
	         const enum type &type,
	         const long &monotonic,
	         const std::array<long, 3> &semantic,
	         const std::function<void (versions &, const mutable_buffer &)> &generator);

	versions() = default;
	versions(versions &&) = delete;
	versions(const versions &) = delete;
	~versions() noexcept;
};

inline ircd::info::versions::operator
const long &()
const
{
	return monotonic?: semantic[0];
}

inline ircd::info::versions::operator
ircd::string_view()
const
{
	return string;
}

inline auto &
ircd::info::versions::operator[](const int &idx)
const
{
	return semantic.at(idx);
}
