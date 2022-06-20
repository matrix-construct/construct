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
	template<bool fenced = false>
	struct scope_cycles;
}

/// Count the reference cycles for a scope using the lifetime of this object.
/// The result is added to the value in `result`; note that result must be
/// initialized.
///
/// The 'fenced' option in the template enables fence instructions as
/// prescribed by amd64 manuals. The fences provide a more accurate result for
/// the section being counted at the cost of additional general overhead. This
/// is disabled by default so that permanent instances of this device are not
/// interfering and don't require any templating.
///
template<bool fenced>
struct ircd::prof::scope_cycles
{
	uint64_t &result;
	uint64_t started;

	scope_cycles(uint64_t &result) noexcept;
	scope_cycles(const scope_cycles &) = delete;
	~scope_cycles() noexcept;
};

template<bool fenced>
#ifdef __clang__
inline //TODO: ???
#else
extern inline
__attribute__((always_inline, gnu_inline, artificial))
#endif
ircd::prof::scope_cycles<fenced>::scope_cycles(uint64_t &result)
noexcept
:result{result}
{
	if constexpr(fenced)
	{
		#if defined(__x86_64__) || defined(__i386__)
		asm volatile ("mfence");
		asm volatile ("lfence");
		#endif
	}

	started = cycles();

	if constexpr(fenced)
	{
		#if defined(__x86_64__) || defined(__i386__)
		asm volatile ("lfence");
		#endif
	}
}

template<bool fenced>
#ifdef __clang__
inline //TODO: ???
#else
extern inline
__attribute__((always_inline, gnu_inline, artificial))
#endif
ircd::prof::scope_cycles<fenced>::~scope_cycles()
noexcept
{
	if constexpr(fenced)
	{
		#if defined(__x86_64__) || defined(__i386__)
		asm volatile ("mfence");
		asm volatile ("lfence");
		#endif
	}

	const uint64_t stopped(cycles());
	result += stopped - started;

	if constexpr(fenced)
	{
		#if defined(__x86_64__) || defined(__i386__)
		asm volatile ("lfence");
		#endif
	}
}
