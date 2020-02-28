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
#define HAVE_IRCD_CTX_THIS_CTX_H

/// Interface to the currently running context
namespace ircd::ctx { inline namespace this_ctx
{
	struct ctx &cur() noexcept;                  ///< Assumptional reference to *current
	const uint64_t &id() noexcept;               // Unique ID for cur ctx
	string_view name() noexcept;                 // Optional label for cur ctx
	ulong cycles() noexcept;                     // misc profiling related
	bool interruption_requested() noexcept;      // interruption(cur())
	void interruption_point();                   // throws if interruption_requested()
	void yield();                                // Allow other contexts to run before returning.
}}

namespace ircd::ctx
{
	/// Points to the currently running context or null for main stack (do not modify)
	extern thread_local ctx *current;
}

namespace ircd
{
	namespace this_ctx = ctx::this_ctx;
}

/// View the name of the currently running context, or "*" if no context is
/// currently running.
inline ircd::string_view
ircd::ctx::this_ctx::name()
noexcept
{
	return current? name(cur()) : "*"_sv;
}

/// Calculate the current TSC (reference cycle count) accumulated for this
/// context only. This is done by first calculating a cycle count for the
/// current slice/epoch (see: ctx/prof.h) which is where the RDTSC sample
/// occurs. This count is added to an accumulator value saved in the ctx
/// structure. The accumulator value is updated at the end of each execution
/// slice, thus giving us the cycle count for this ctx only, up to this point.
extern inline ulong
__attribute__((flatten, always_inline, gnu_inline, artificial))
ircd::ctx::this_ctx::cycles()
noexcept
{
	const auto slice(prof::cur_slice_cycles());
	const auto accumulated(cycles(cur()));
	return accumulated + slice;
}

/// Reference to the currently running context. Call if you expect to be in a
/// context. Otherwise use the ctx::current pointer.
inline ircd::ctx::ctx &
__attribute__((always_inline))
ircd::ctx::this_ctx::cur()
noexcept
{
	assert(current);
	return *current;
}
