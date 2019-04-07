// Matrix Construct
//
// Copyright (C) Matrix Construct Developers, Authors & Contributors
// Copyright (C) 2016-2019 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

#pragma once
#define HAVE_IRCD_PROF_H

namespace ircd::prof
{
	struct init;
	struct type;
	struct event;
	struct times;
	struct system;
	enum dpl :uint8_t;
	enum counter :uint8_t;
	enum cacheop :uint8_t;
	using group = std::vector<std::unique_ptr<event>>;
	IRCD_OVERLOAD(sample)
	IRCD_EXCEPTION(ircd::error, error)

	uint64_t cycles();      ///< Monotonic reference cycles (since system boot)
	uint64_t time_user();   ///< Nanoseconds of CPU time in userspace.
	uint64_t time_kern();   ///< Nanoseconds of CPU time in kernelland.
	uint64_t time_real();   ///< Nanoseconds of CPU time real.
	uint64_t time_proc();   ///< Nanoseconds of CPU time for process.
	uint64_t time_thrd();   ///< Nanoseconds of CPU time for thread.

	// Observe
	system &hotsample(system &) noexcept;
	system &operator+=(system &a, const system &b);
	system &operator-=(system &a, const system &b);
	system operator+(const system &a, const system &b);
	system operator-(const system &a, const system &b);

	// Control
	void stop(group &);
	void start(group &);
	void reset(group &);
}

/// X86 platform related
namespace ircd::prof::x86
{
	unsigned long long rdpmc(const uint &);
	unsigned long long rdtscp();
	unsigned long long rdtsc();
}

namespace ircd
{
	using prof::cycles;
}

struct ircd::prof::times
{
	uint64_t real {0};
	uint64_t kern {0};
	uint64_t user {0};

	times(sample_t);
	times() = default;
};

struct ircd::prof::system
:std::array<std::array<uint64_t, 2>, 7>
{
	using array_type = std::array<std::array<uint64_t, 2>, 7>;

	static prof::group group;

	// [N][0] = KERNEL, [N][1] = USER
	//
	// 0: TIME_PROF,
	// 1: TIME_CPU,
	// 2: TIME_TASK,
	// 3: PF_MINOR,
	// 4: PF_MAJOR,
	// 5: SWITCH_TASK,
	// 6: SWITCH_CPU,

	system(sample_t) noexcept;
	system()
	:array_type{{0}}
	{}
};

struct ircd::prof::type
{
	enum dpl dpl {0};
	enum counter counter {0};
	enum cacheop cacheop {0};

	type(const event &);
	type(const enum dpl & = (enum dpl)0,
	     const enum counter & = (enum counter)0,
	     const enum cacheop & = (enum cacheop)0);
};

enum ircd::prof::dpl
:std::underlying_type<ircd::prof::dpl>::type
{
	KERNEL,
	USER
};

enum ircd::prof::counter
:std::underlying_type<ircd::prof::counter>::type
{
	TIME_PROF,
	TIME_CPU,
	TIME_TASK,
	PF_MINOR,
	PF_MAJOR,
	SWITCH_TASK,
	SWITCH_CPU,

	CYCLES,
	RETIRES,
	BRANCHES,
	BRANCHES_MISS,
	CACHES,
	CACHES_MISS,
	STALLS_READ,
	STALLS_RETIRE,

	CACHE_L1D,
	CACHE_L1I,
	CACHE_LL,
	CACHE_TLBD,
	CACHE_TLBI,
	CACHE_BPU,
	CACHE_NODE,

	_NUM
};

enum ircd::prof::cacheop
:std::underlying_type<ircd::prof::cacheop>::type
{
	READ_ACCESS,
	READ_MISS,
	WRITE_ACCESS,
	WRITE_MISS,
	PREFETCH_ACCESS,
	PREFETCH_MISS,
};

struct ircd::prof::init
{
	init();
	~init() noexcept;
};

#if defined(__x86_64__) || defined(__i386__)
inline uint64_t
__attribute__((flatten, always_inline, gnu_inline, artificial))
ircd::prof::cycles()
{
	return x86::rdtsc();
}
#else
ircd::prof::cycles()
{
	static_assert(false, "Select reference cycle counter for platform.");
	return 0;
}
#endif

#if defined(__x86_64__) || defined(__i386__)
inline unsigned long long
__attribute__((always_inline, gnu_inline, artificial))
ircd::prof::x86::rdtsc()
{
	return __builtin_ia32_rdtsc();
}
#else
inline unsigned long long
ircd::prof::x86::rdtsc()
{
	return 0;
}
#endif

#if defined(__x86_64__) || defined(__i386__)
inline unsigned long long
__attribute__((always_inline, gnu_inline, artificial))
ircd::prof::x86::rdtscp()
{
	uint32_t ia32_tsc_aux;
	return __builtin_ia32_rdtscp(&ia32_tsc_aux);
}
#else
inline unsigned long long
ircd::prof::x86::rdtscp()
{
	return 0;
}
#endif

#if defined(__x86_64__) || defined(__i386__)
inline unsigned long long
__attribute__((always_inline, gnu_inline, artificial))
ircd::prof::x86::rdpmc(const uint &c)
{
	return __builtin_ia32_rdpmc(c);
}
#else
inline unsigned long long
ircd::prof::x86::rdpmc(const uint &c)
{
	return 0;
}
#endif
