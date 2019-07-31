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
#define HAVE_IRCD_PROF_X86_H

/// X86 platform related
namespace ircd::prof::x86
{
	unsigned long long rdpmc(const uint &);
	unsigned long long rdtscp();
	unsigned long long rdtsc();
}

#if defined(__x86_64__) || defined(__i386__)
extern inline unsigned long long
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
extern inline unsigned long long
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
extern inline unsigned long long
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
