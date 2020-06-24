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
#define HAVE_IRCD_PROF_SCOPE_CYCLES_H

namespace ircd::prof
{
	struct scope_cycles;
}

/// Count the reference cycles for a scope using the lifetime of this object.
/// The result is added to the value in `result`; note that result must be
/// initialized.
struct ircd::prof::scope_cycles
{
	uint64_t &result;
	uint64_t started;

	scope_cycles(uint64_t &result) noexcept;
	scope_cycles(const scope_cycles &) = delete;
	~scope_cycles() noexcept;
};

#ifdef __clang__
inline //TODO: ???
#else
extern inline
__attribute__((flatten, always_inline, gnu_inline, artificial))
#endif
ircd::prof::scope_cycles::scope_cycles(uint64_t &result)
noexcept
:result{result}
{
	#if defined(__x86_64__) || defined(__i386__)
	asm volatile ("mfence");
	asm volatile ("lfence");
	#endif

	started = cycles();

	#if defined(__x86_64__) || defined(__i386__)
	asm volatile ("lfence");
	#endif
}

#ifdef __clang__
inline //TODO: ???
#else
extern inline
__attribute__((flatten, always_inline, gnu_inline, artificial))
#endif
ircd::prof::scope_cycles::~scope_cycles()
noexcept
{
	#if defined(__x86_64__) || defined(__i386__)
	asm volatile ("mfence");
	asm volatile ("lfence");
	#endif

	const uint64_t stopped(cycles());
	result += stopped - started;

	#if defined(__x86_64__) || defined(__i386__)
	asm volatile ("lfence");
	#endif
}
