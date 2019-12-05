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
#define HAVE_IRCD_UTIL_UNWIND_H

namespace ircd
{
	inline namespace util
	{
		template<class F> struct unwind;
		template<class F> struct unwind_nominal;
		template<class F> struct unwind_exceptional;
		struct unwind_defer;
		struct unwind_nominal_assertion;
		struct unwind_exceptional_assertion;
	}
}

//
// Fundamental scope-unwind utilities establishing actions during destruction
//

/// Unconditionally executes the provided code when the object goes out of scope.
///
template<class F>
struct ircd::util::unwind
{
	const F func;

	unwind(F&& func)
	:func{std::forward<F>(func)}
	{}

	unwind(const unwind &) = delete;
	unwind &operator=(const unwind &) = delete;
	~unwind() noexcept
	__attribute__((always_inline))
	{
		func();
	}
};

/// Executes function only if the unwind takes place without active exception
///
/// The function is expected to be executed and the likely() should pipeline
/// that branch and make this device cheaper to use under normal circumstances.
///
template<class F>
struct ircd::util::unwind_nominal
{
	const F func;

	unwind_nominal(F&& func)
	:func{std::forward<F>(func)}
	{}

	~unwind_nominal() noexcept
	__attribute__((always_inline))
	{
		if(likely(!std::uncaught_exceptions()))
			func();
	}

	unwind_nominal(const unwind_nominal &) = delete;
};

/// Executes function only if unwind is taking place because exception thrown
///
/// The unlikely() intends for the cost of a branch misprediction to be paid
/// for fetching and executing this function. This is because we strive to
/// optimize the pipeline for the nominal path, making this device as cheap
/// as possible to use.
///
template<class F>
struct ircd::util::unwind_exceptional
{
	const F func;

	unwind_exceptional(F&& func)
	:func{std::forward<F>(func)}
	{}

	~unwind_exceptional() noexcept
	__attribute__((always_inline))
	{
		if(unlikely(std::uncaught_exceptions()))
			func();
	}

	unwind_exceptional(const unwind_exceptional &) = delete;
};

/// Posts function to the event loop at the unwind rather than executing it
/// as with nominal/exceptional.
///
struct ircd::util::unwind_defer
{
	std::function<void ()> func;

	template<class F>
	unwind_defer(F&& func)
	:func{std::forward<F>(func)}
	{}

	unwind_defer(const unwind_defer &) = delete;
	~unwind_defer() noexcept;
};

/// Asserts that unwind is occurring without any exception. This allows some
/// section of code, for example, the latter half of a function, to assert that
/// it is not throwing and disrupting the other half. This is useful when no
/// exception is expected thus placing other guarantees should not be required
/// if this guarantee is met.
///
struct ircd::util::unwind_nominal_assertion
{
	~unwind_nominal_assertion() noexcept
	{
		assert(likely(!std::uncaught_exceptions()));
	}
};

/// Complements exceptional::assertion. This triggers an assertion when
/// unwinding WITHOUT an exception.
///
struct ircd::util::unwind_exceptional_assertion
{
	~unwind_exceptional_assertion() noexcept
	{
		assert(likely(std::uncaught_exceptions()));
	}
};
