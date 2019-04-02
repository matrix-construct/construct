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

	IRCD_EXCEPTION(ircd::error, error)

	unsigned long long rdpmc(const uint &);
	unsigned long long rdtscp();
	unsigned long long rdtsc();

	uint64_t cycles();
}

namespace ircd
{
	using prof::cycles;
}

struct ircd::prof::init
{
	init();
	~init() noexcept;
};

inline uint64_t
__attribute__((flatten, always_inline, gnu_inline, artificial))
ircd::prof::cycles()
{
	return prof::rdtsc();
}

#if defined(__x86_64__) || defined(__i386__)
inline unsigned long long
__attribute__((always_inline, gnu_inline, artificial))
ircd::prof::rdtsc()
{
	return __builtin_ia32_rdtsc();
}
#else
inline unsigned long long
ircd::prof::rdtsc()
{
	static_assert(false, "TODO: Implement fallback here");
	return 0;
}
#endif

#if defined(__x86_64__) || defined(__i386__)
inline unsigned long long
__attribute__((always_inline, gnu_inline, artificial))
ircd::prof::rdtscp()
{
	uint32_t ia32_tsc_aux;
	return __builtin_ia32_rdtscp(&ia32_tsc_aux);
}
#else
inline unsigned long long
ircd::prof::rdtscp()
{
	static_assert(false, "TODO: Implement fallback here");
	return 0;
}
#endif

#if defined(__x86_64__) || defined(__i386__)
inline unsigned long long
__attribute__((always_inline, gnu_inline, artificial))
ircd::prof::rdpmc(const uint &c)
{
	return __builtin_ia32_rdpmc(c);
}
#else
inline unsigned long long
ircd::prof::rdpmc(const uint &c)
{
	static_assert(false, "TODO: Implement fallback here");
	return 0;
}
#endif
