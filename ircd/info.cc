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
#include <RB_INC_SYS_AUXV_H
#include <RB_INC_SYS_SYSINFO_H
#include <RB_INC_GNU_LIBC_VERSION_H

namespace ircd::info
{
	static void dump_exe_info();
	static void dump_lib_info();
	static void dump_sys_info();
	static void dump_cpu_info_x86();
	static void dump_cpu_info_arm();
	static void dump_cpu_info();
}

decltype(ircd::info::credits)
ircd::info::credits
{
//	"Load bearing comment"
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
	if(closure) try
	{
		closure(*this, this->string);
	}
	catch(const std::exception &e)
	{
		log::error
		{
			"Querying %s version of '%s' :%s",
			type == type::ABI? "ABI"_sv : "API"_sv,
			name,
			e.what(),
		};
	}

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
	dump_cpu_info_x86();
	dump_cpu_info_arm();

	char pbuf[6][48];
	log::info
	{
		log::star,
		"L1i %s L1d %s L2 %s L3 %s RAM %s SWAP %s",
		pretty(pbuf[0], iec(hardware::l1i)),
		pretty(pbuf[1], iec(hardware::l1d)),
		pretty(pbuf[2], iec(hardware::l2)),
		pretty(pbuf[3], iec(hardware::l3)),
		pretty(pbuf[4], iec(total_ram)),
		pretty(pbuf[5], iec(total_swap)),
	};

	if(!ircd::debugmode)
		return;

	log::logf
	{
		log::star, log::DEBUG,
		"L1i %s line=%u assoc=%u line/tag=%u tlb=%u assoc=%u ",
		pretty(pbuf[0], iec(hardware::l1i)),
		hardware::l1i_line,
		hardware::l1i_assoc,
		hardware::l1i_tag,
		hardware::l1i_tlb,
		hardware::l1i_tlb_assoc,
	};

	log::logf
	{
		log::star, log::DEBUG,
		"L1d %s line=%u assoc=%u line/tag=%u tlb=%u assoc=%u",
		pretty(pbuf[0], iec(hardware::l1d)),
		hardware::l1d_line,
		hardware::l1d_assoc,
		hardware::l1d_tag,
		hardware::l1d_tlb,
		hardware::l1d_tlb_assoc,
	};

	log::logf
	{
		log::star, log::DEBUG,
		"L2 %s line=%u assoc=%u line/tag=%u itlb=%u assoc=%u dtlb=%u assoc=%u",
		pretty(pbuf[0], iec(hardware::l2)),
		hardware::l2_line,
		hardware::l2_assoc,
		hardware::l2_tag,
		hardware::l2_itlb,
		hardware::l2_itlb_assoc,
		hardware::l2_dtlb,
		hardware::l2_dtlb_assoc,
	};

	log::logf
	{
		log::star, log::DEBUG,
		"L3 %s line=%u assoc=%u line/tag=%u",
		pretty(pbuf[0], iec(hardware::l3)),
		hardware::l3_line,
		hardware::l3_assoc,
		hardware::l3_tag,
	};

	// This message flashes language standard information about this platform
	log::logf
	{
		log::star, log::DEBUG,
		"max_align=%zu hw_conc=%zu d_inter=%zu c_inter=%zu "
		"inst_blksz=%zu data_blksz=%zu uni_blksz=%zu page_size=%zu",
		hardware::max_align,
		hardware::hardware_concurrency,
		hardware::destructive_interference,
		hardware::constructive_interference,
		hardware::inst_blksz,
		hardware::data_blksz,
		hardware::uni_blksz,
		hardware::page_size,
	};
}

void
ircd::info::dump_cpu_info_arm()
#if defined(__aarch64__)
{
	log::info
	{
		log::star, "aarch64 %s MIDR[%08lx] REVIDR[%08lx] PFR0[%016lx] ISAR0[%016lx] MMFR0[%016lx] CACHETYPE[%016lx]",
		hardware::arm::vendor,
		hardware::arm::midr,
		hardware::arm::revidr,
		hardware::arm::isar[0],
		hardware::arm::mmfr[0],
		hardware::arm::pfr[0],
		hardware::arm::ctr,
	};
}
#else
{
}
#endif

void
ircd::info::dump_cpu_info_x86()
#if defined(__x86_64__)
{
	char support[128] {0};
	const auto append{[&support]
	(const string_view &name, const bool &avail, const int &enable)
	{
		strlcat(support, fmt::bsprintf<64>
		{
			" %s:%c%s",
			name,
			avail == true? 'y': 'n',
			enable == true? "y": enable == false? "n": "",
		});
	}};

	append("sse2", hardware::x86::sse2, simd::support::sse2);
	append("sse3", hardware::x86::sse3, simd::support::sse3);
	append("ssse3", hardware::x86::ssse3, simd::support::ssse3);
	append("sse4a", hardware::x86::sse4a, simd::support::sse4a);
	append("sse4.1", hardware::x86::sse4_1, simd::support::sse4_1);
	append("sse4.2", hardware::x86::sse4_2, simd::support::sse4_2);
	append("avx", hardware::x86::avx, simd::support::avx);
	append("avx2", hardware::x86::avx2, simd::support::avx2);
	append("avx512f", hardware::x86::avx512f, simd::support::avx512f);
	append("constant_tsc", hardware::x86::tsc_constant, -1);

	log::info
	{
		log::star, "x86_64 %s %s%s%s",
		hardware::x86::vendor,
		hardware::virtualized?
			"virtual"_sv:
			"physical"_sv,
		vg::active?
			" valgrind"_sv:
			""_sv,
		support,
	};

	log::logf
	{
		log::star, log::DEBUG,
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

	log::logf
	{
		log::star, log::DEBUG,
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

	log::logf
	{
		log::star, log::DEBUG,
		"8..05 EXT L1CACHE [%08x|%08x|%08x|%08x] "
		"8..06 EXT LLCACHE [%08x|%08x|%08x|%08x]",
		uint32_t(hardware::x86::_l1cache >> 0),
		uint32_t(hardware::x86::_l1cache >> 32),
		uint32_t(hardware::x86::_l1cache >> 64),
		uint32_t(hardware::x86::_l1cache >> 96),
		uint32_t(hardware::x86::_llcache >> 0),
		uint32_t(hardware::x86::_llcache >> 32),
		uint32_t(hardware::x86::_llcache >> 64),
		uint32_t(hardware::x86::_llcache >> 96),
	};

	log::logf
	{
		log::star, log::DEBUG,
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
#else
{
}
#endif

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

decltype(ircd::info::hardware::x86::_l1cache)
ircd::info::hardware::x86::_l1cache
{
	cpuid(0x80000005U, 0)
};

decltype(ircd::info::hardware::x86::_llcache)
ircd::info::hardware::x86::_llcache
{
	cpuid(0x80000006U, 0)
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

static ircd::fixed_buffer<ircd::const_buffer, 16>
ircd_info_hardware_x86_vendor{[](const auto &out)
{
	const auto in
	{
		reinterpret_cast<const uint8_t *>(&ircd::info::hardware::x86::manufact)
	};

	out[0] = in[4];  out[1] = in[5];  out[2] = in[6];   out[3] = in[7];
	out[4] = in[12]; out[5] = in[13]; out[6] = in[14];  out[7] = in[15];
	out[8] = in[8];  out[9] = in[9];  out[10] = in[10]; out[11] = in[11];
}};

decltype(ircd::info::hardware::x86::vendor)
ircd::info::hardware::x86::vendor
{
	ircd_info_hardware_x86_vendor
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

decltype(ircd::info::hardware::x86::sse4a)
ircd::info::hardware::x86::sse4a
{
	bool(_features & (uint128_t(1) << (64 + 6)))
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

decltype(ircd::info::hardware::x86::avx512f)
ircd::info::hardware::x86::avx512f
{
	bool(features & (uint128_t(1) << (32 + 16)))
};

decltype(ircd::info::hardware::x86::tsc)
ircd::info::hardware::x86::tsc
{
	bool(features & (uint128_t(1) << 4))
};

decltype(ircd::info::hardware::x86::tsc_constant)
ircd::info::hardware::x86::tsc_constant
{
	bool(_apmi & (uint128_t(1) << (8)))
};

#ifdef __x86_64__
ircd::uint128_t
ircd::info::hardware::x86::cpuid(const uint &leaf,
                                 const uint &subleaf)
noexcept
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
noexcept
{
	return 0;
}
#endif

/// AMD64 CPUID Rev. 2.34 (Sept 2010) - Table 4
uint8_t
ircd::info::hardware::x86::llc_assoc(const uint8_t a)
noexcept
{
	switch(a)
	{
		default:
		case 0x0:   return 0;
		case 0x1:   return 1;
		case 0x2:   return 2;
		case 0x4:   return 4;
		case 0x6:   return 8;
		case 0x8:   return 16;
		case 0xA:   return 32;
		case 0xB:   return 48;
		case 0xC:   return 64;
		case 0xD:   return 96;
		case 0xE:   return 128;
		case 0xF:   return -1;
	}
}

//
// aarch64
//
#if defined(__aarch64__)

decltype(ircd::info::hardware::arm::midr)
ircd::info::hardware::arm::midr{[]
{
	uint64_t ret {0};
	#if defined(__aarch64__)
	asm volatile ("mrs %0, MIDR_EL1": "=r" (ret));
	#endif
	return ret;
}()};

decltype(ircd::info::hardware::arm::revidr)
ircd::info::hardware::arm::revidr{[]
{
	uint64_t ret {0};
	#if defined(__aarch64__)
	asm volatile ("mrs %0, REVIDR_EL1": "=r" (ret));
	#endif
	return ret;
}()};

decltype(ircd::info::hardware::arm::isar)
ircd::info::hardware::arm::isar{[]
{
	uint64_t ret {0};
	#if defined(__aarch64__)
	asm volatile ("mrs %0, ID_AA64ISAR0_EL1": "=r" (ret));
	#endif
	return ret;
}()};

decltype(ircd::info::hardware::arm::mmfr)
ircd::info::hardware::arm::mmfr{[]
{
	uint64_t ret {0};
	#if defined(__aarch64__)
	asm volatile ("mrs %0, ID_AA64MMFR0_EL1": "=r" (ret));
	#endif
	return ret;
}()};

decltype(ircd::info::hardware::arm::pfr)
ircd::info::hardware::arm::pfr{[]
{
	uint64_t ret {0};
	#if defined(__aarch64__)
	asm volatile ("mrs %0, ID_AA64PFR0_EL1": "=r" (ret));
	#endif
	return ret;
}()};

decltype(ircd::info::hardware::arm::ctr)
ircd::info::hardware::arm::ctr{[]
{
	uint64_t ret {0};
	#if defined(__aarch64__)
	asm volatile ("mrs %0, CTR_EL0": "=r" (ret));
	#endif
	return ret;
}()};

static ircd::fixed_buffer<ircd::const_buffer, 32>
ircd_info_hardware_arm_vendor{[](const auto &out)
{
	ircd::copy(out, ircd::string_view("<unknown vendor>"));
}};

decltype(ircd::info::hardware::arm::vendor)
ircd::info::hardware::arm::vendor
{
	ircd_info_hardware_arm_vendor
};

#endif // __aarch64__

//
// Generic / Standard
//

decltype(ircd::info::hardware::arch)
ircd::info::hardware::arch
{
	#if defined(__x86_64__)
		"x86_64"
	#elif defined(__aarch64__)
		"aarch64"
	#else
		"undefined"
	#endif
};

decltype(ircd::info::hardware::endian)
ircd::info::hardware::endian
{
	#if __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
		"big"
	#elif __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
		"little"
	#else
		"undefined"
	#endif
};

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

decltype(ircd::info::hardware::inst_blksz)
ircd::info::hardware::inst_blksz
{
	#if __has_include(<sys/auxv.h>) && defined(AT_ICACHEBSIZE)
		getauxval(AT_ICACHEBSIZE)
	#endif
};

decltype(ircd::info::hardware::data_blksz)
ircd::info::hardware::data_blksz
{
	#if __has_include(<sys/auxv.h>) && defined(AT_DCACHEBSIZE)
		getauxval(AT_DCACHEBSIZE)
	#endif
};

decltype(ircd::info::hardware::uni_blksz)
ircd::info::hardware::uni_blksz
{
	#if __has_include(<sys/auxv.h>) && defined(AT_UCACHEBSIZE)
		getauxval(AT_UCACHEBSIZE)
	#endif
};

decltype(ircd::info::hardware::l1i)
ircd::info::hardware::l1i
{
	#ifdef __x86_64__
		uint8_t((x86::_l1cache >> (96 + 24)) & 0xffUL) * 1024UL
	#elif __has_include(<sys/auxv.h>) && defined(AT_L1I_CACHESIZE)
		getauxval(AT_L1I_CACHESIZE)
	#endif
};

decltype(ircd::info::hardware::l1i_line)
ircd::info::hardware::l1i_line
{
	#ifdef __x86_64__
		uint8_t((x86::_l1cache >> (96 + 0)) & 0xffUL)
	#elif __has_include(<sys/auxv.h>) && defined(AT_L1I_CACHEGEOMETRY)
		getauxval(AT_L1I_CACHEGEOMETRY) & 0xffffUL
	#endif
};

decltype(ircd::info::hardware::l1i_tag)
ircd::info::hardware::l1i_tag
{
	#ifdef __x86_64__
		uint8_t((x86::_l1cache >> (96 + 8)) & 0xffUL)
	#endif
};

decltype(ircd::info::hardware::l1i_assoc)
ircd::info::hardware::l1i_assoc
{
	#ifdef __x86_64__
		uint8_t((x86::_l1cache >> (96 + 16)) & 0xffUL)
	#elif __has_include(<sys/auxv.h>) && defined(AT_L1I_CACHEGEOMETRY)
		getauxval(AT_L1I_CACHEGEOMETRY) & 0xffff0000UL
	#endif
};

decltype(ircd::info::hardware::l1i_tlb)
ircd::info::hardware::l1i_tlb
{
	#ifdef __x86_64__
		uint8_t((x86::_l1cache >> (32 + 0)) & 0xffUL)
	#endif
};

decltype(ircd::info::hardware::l1i_tlb_assoc)
ircd::info::hardware::l1i_tlb_assoc
{
	#ifdef __x86_64__
		uint8_t((x86::_l1cache >> (32 + 8)) & 0xffUL)
	#endif
};

decltype(ircd::info::hardware::l1d)
ircd::info::hardware::l1d
{
	#ifdef __x86_64__
		uint8_t((x86::_l1cache >> (64 + 24)) & 0xffUL) * 1024UL
	#elif __has_include(<sys/auxv.h>) && defined(AT_L1D_CACHESIZE)
		getauxval(AT_L1D_CACHESIZE)
	#endif
};

decltype(ircd::info::hardware::l1d_line)
ircd::info::hardware::l1d_line
{
	#ifdef __x86_64__
		uint8_t((x86::_l1cache >> (64 + 0)) & 0xffUL)
	#elif __has_include(<sys/auxv.h>) && defined(AT_L1D_CACHEGEOMETRY)
		getauxval(AT_L1D_CACHEGEOMETRY) & 0xffffUL
	#endif
};

decltype(ircd::info::hardware::l1d_tag)
ircd::info::hardware::l1d_tag
{
	#ifdef __x86_64__
		uint8_t((x86::_l1cache >> (64 + 8)) & 0xffUL)
	#endif
};

decltype(ircd::info::hardware::l1d_assoc)
ircd::info::hardware::l1d_assoc
{
	#ifdef __x86_64__
		uint8_t((x86::_l1cache >> (64 + 16)) & 0xffUL)
	#elif __has_include(<sys/auxv.h>) && defined(AT_L1D_CACHEGEOMETRY)
		getauxval(AT_L1D_CACHEGEOMETRY) & 0xffff0000UL
	#endif
};

decltype(ircd::info::hardware::l1d_tlb)
ircd::info::hardware::l1d_tlb
{
	#ifdef __x86_64__
		uint8_t((x86::_l1cache >> (32 + 16)) & 0xffUL)
	#endif
};

decltype(ircd::info::hardware::l1d_tlb_assoc)
ircd::info::hardware::l1d_tlb_assoc
{
	#ifdef __x86_64__
		uint8_t((x86::_l1cache >> (32 + 24)) & 0xffUL)
	#endif
};

decltype(ircd::info::hardware::l2)
ircd::info::hardware::l2
{
	#ifdef __x86_64__
		uint16_t((x86::_llcache >> (64 + 16)) & 0xffffUL) * 1024UL
	#elif __has_include(<sys/auxv.h>) && defined(AT_L2_CACHESIZE)
		getauxval(AT_L2_CACHESIZE)
	#endif
};

decltype(ircd::info::hardware::l2_line)
ircd::info::hardware::l2_line
{
	#ifdef __x86_64__
		uint8_t((x86::_llcache >> (64 + 0)) & 0xffUL)
	#elif __has_include(<sys/auxv.h>) && defined(AT_L2_CACHEGEOMETRY)
		getauxval(AT_L2_CACHEGEOMETRY) & 0xffffUL
	#endif
};

decltype(ircd::info::hardware::l2_tag)
ircd::info::hardware::l2_tag
{
	#ifdef __x86_64__
		uint8_t((x86::_llcache >> (64 + 8)) & 0x0fUL)
	#endif
};

decltype(ircd::info::hardware::l2_assoc)
ircd::info::hardware::l2_assoc
{
	#ifdef __x86_64__
		x86::llc_assoc((x86::_llcache >> (64 + 12)) & 0x0fUL)
	#elif __has_include(<sys/auxv.h>) && defined(AT_L2_CACHEGEOMETRY)
		getauxval(AT_L2_CACHEGEOMETRY) & 0xffff0000UL
	#endif
};

decltype(ircd::info::hardware::l2_itlb)
ircd::info::hardware::l2_itlb
{
	#ifdef __x86_64__
		uint16_t((x86::_llcache >> (32 + 0)) & 0x0fffUL)
	#endif
};

decltype(ircd::info::hardware::l2_itlb_assoc)
ircd::info::hardware::l2_itlb_assoc
{
	#ifdef __x86_64__
		x86::llc_assoc(uint8_t((x86::_llcache >> (32 + 12)) & 0x0fUL))
	#endif
};

decltype(ircd::info::hardware::l2_dtlb)
ircd::info::hardware::l2_dtlb
{
	#ifdef __x86_64__
		uint16_t((x86::_llcache >> (32 + 16)) & 0x0fffUL)
	#endif
};

decltype(ircd::info::hardware::l2_dtlb_assoc)
ircd::info::hardware::l2_dtlb_assoc
{
	#ifdef __x86_64__
		x86::llc_assoc(uint8_t((x86::_llcache >> (32 + 28)) & 0x0fUL))
	#endif
};

decltype(ircd::info::hardware::l3)
ircd::info::hardware::l3
{
	#ifdef __x86_64__
		(uint16_t((x86::_llcache >> (96 + 16)) & 0xffffUL) >> 2) * 512_KiB
	#elif __has_include(<sys/auxv.h>) && defined(AT_L3_CACHESIZE)
		getauxval(AT_L3_CACHESIZE)
	#endif
};

decltype(ircd::info::hardware::l3_line)
ircd::info::hardware::l3_line
{
	#ifdef __x86_64__
		uint8_t((x86::_llcache >> (96 + 0)) & 0xffUL)
	#elif __has_include(<sys/auxv.h>) && defined(AT_L3_CACHEGEOMETRY)
		getauxval(AT_L3_CACHEGEOMETRY) & 0xffffUL
	#endif
};

decltype(ircd::info::hardware::l3_tag)
ircd::info::hardware::l3_tag
{
	#ifdef __x86_64__
		uint8_t((x86::_llcache >> (96 + 8)) & 0x0fUL)
	#endif
};

decltype(ircd::info::hardware::l3_assoc)
ircd::info::hardware::l3_assoc
{
	#ifdef __x86_64__
		x86::llc_assoc((x86::_llcache >> (96 + 12)) & 0x0fUL)
	#elif __has_include(<sys/auxv.h>) && defined(AT_L3_CACHEGEOMETRY)
		getauxval(AT_L3_CACHEGEOMETRY) & 0xffff0000UL
	#endif
};

decltype(ircd::info::hardware::page_size)
ircd::info::hardware::page_size
{
	#if __has_include(<sys/auxv.h>) && defined(AT_PAGESZ)
		getauxval(AT_PAGESZ)
	#endif
};

decltype(ircd::info::hardware::cap)
ircd::info::hardware::cap
{
	#if __has_include(<sys/auxv.h>) && defined(AT_HWCAP)
		getauxval(AT_HWCAP)
	#else
		0
	#endif
	,
	#if __has_include(<sys/auxv.h>) && defined(AT_HWCAP2)
		getauxval(AT_HWCAP2)
	#else
		0UL
	#endif
};

decltype(ircd::info::hardware::virtualized)
ircd::info::hardware::virtualized
{
	#ifdef __x86_64__
		bool(x86::features & (uint128_t(1) << (64 + 31)))
	#endif
};

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
	//#ifdef RB_DEBUG
	char buf[2][48];
	log::logf
	{
		log::star, log::DEBUG,
		"page_size=%zu iov_max=%zd aio_max=%zd aio_reqprio_max=%zd memlock_limit=%s clock_source=%s thp=%s:%zu",
		page_size,
		iov_max,
		aio_max,
		aio_reqprio_max,
		pretty(buf[0], iec(allocator::rlimit_memlock())),
		clock_source,
		between(thp_enable, '[', ']'),
		thp_size,
	};
	//#endif
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

decltype(ircd::info::vsyscall_p)
ircd::info::vsyscall_p
{
	#if __has_include(<sys/auxv.h>) && defined(AT_SYSINFO)
		intptr_t(getauxval(AT_SYSINFO))
	#endif
};

decltype(ircd::info::vdso_p)
ircd::info::vdso_p
{
	#if __has_include(<sys/auxv.h>) && defined(AT_SYSINFO_EHDR)
		intptr_t(getauxval(AT_SYSINFO_EHDR))
	#endif
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

static char ircd_info_clock_source[32];
decltype(ircd::info::clock_source)
ircd::info::clock_source
{
	sys::get(ircd_info_clock_source, "devices/system/clocksource/clocksource0/current_clocksource")
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
		// no syscall() wrapper; -1 becomes unlimited
		size_t(::sysconf(_SC_AIO_MAX))
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

decltype(ircd::info::thp_size)
ircd::info::thp_size
{
	sys::get<size_t>("kernel/mm/transparent_hugepage/hpage_pmd_size", 0UL)
};

static char ircd_info_thp_enable_buf[128];
decltype(ircd::info::thp_enable)
ircd::info::thp_enable
{
	thp_size?
		sys::get(ircd_info_thp_enable_buf, "kernel/mm/transparent_hugepage/enabled"):
		string_view{}
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
		log::star, "%s %s SD-6 %s. glibcxx %s. glibc %s. boost %s. RocksDB %s. sodium %s. %s. magic %ld.",
		string_view{compiler},
		string_view{RB_CXX_VERSION},
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

decltype(ircd::info::compiler)
ircd::info::compiler
{
	split(RB_CXX, ' ').first
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

extern "C" const char *const
ircd_name
{
	PACKAGE_NAME
};

extern "C" const char *const
ircd_version
{
	PACKAGE_VERSION
};

decltype(ircd::info::name)
ircd::info::name
{
	BRANDING_NAME
};

decltype(ircd::info::version)
ircd::info::version
{
	BRANDING_VERSION
};

decltype(ircd::info::server_agent)
ircd::info::server_agent
{
	BRANDING_NAME "/" BRANDING_VERSION

	" (IRCd "
	#ifdef RB_VERSION_BRANCH
	"b=" RB_VERSION_BRANCH ","
	#endif
	RB_VERSION_COMMIT
	")"
};

decltype(ircd::info::user_agent)
ircd::info::user_agent
{
	server_agent
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

static char ircd_info_configured[32];
decltype(ircd::info::configured)
ircd::info::configured
{
	rstrip(ctime_r(&configured_time, ircd_info_configured), '\n')
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

decltype(ircd::info::startup_time)
ircd::info::startup_time
{
	std::time(nullptr)
};

static char ircd_info_startup[32];
decltype(ircd::info::startup)
ircd::info::startup
{
	rstrip(ctime_r(&startup_time, ircd_info_startup), '\n')
};

decltype(ircd::info::random)
ircd::info::random
{
	#if __has_include(<sys/auxv.h>) && defined(AT_RANDOM)
		getauxval(AT_RANDOM)
	#endif
};

decltype(ircd::info::uid)
ircd::info::uid
{
	#if __has_include(<sys/auxv.h>) && defined(AT_UID)
		uint(getauxval(AT_UID))
	#endif
};

decltype(ircd::info::euid)
ircd::info::euid
{
	#if __has_include(<sys/auxv.h>) && defined(AT_EUID)
		uint(getauxval(AT_EUID))
	#endif
};

decltype(ircd::info::gid)
ircd::info::gid
{
	#if __has_include(<sys/auxv.h>) && defined(AT_GID)
		uint(getauxval(AT_GID))
	#endif
};

decltype(ircd::info::egid)
ircd::info::egid
{
	#if __has_include(<sys/auxv.h>) && defined(AT_EGID)
		uint(getauxval(AT_EGID))
	#endif
};

decltype(ircd::info::secure)
ircd::info::secure
{
	#if __has_include(<sys/auxv.h>) && defined(AT_SECURE)
		bool(getauxval(AT_SECURE))
	#endif
};
