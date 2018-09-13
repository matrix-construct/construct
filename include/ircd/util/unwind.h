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

namespace ircd::util
{
	struct unwind;
};

//
// Fundamental scope-unwind utilities establishing actions during destruction
//

/// Unconditionally executes the provided code when the object goes out of scope.
///
struct ircd::util::unwind
{
	struct nominal;
	struct exceptional;

	const std::function<void ()> func;

	template<class F>
	unwind(F&& func)
	:func{std::forward<F>(func)}
	{}

	unwind(const unwind &) = delete;
	unwind &operator=(const unwind &) = delete;
	~unwind() noexcept
	{
		func();
	}
};

/// Executes function only if the unwind takes place without active exception
///
/// The function is expected to be executed and the likely() should pipeline
/// that branch and make this device cheaper to use under normal circumstances.
///
struct ircd::util::unwind::nominal
{
	struct assertion;

	const std::function<void ()> func;

	template<class F>
	nominal(F&& func)
	:func{std::forward<F>(func)}
	{}

	~nominal() noexcept
	{
		if(likely(!std::uncaught_exceptions()))
			func();
	}

	nominal(const nominal &) = delete;
};

/// Executes function only if unwind is taking place because exception thrown
///
/// The unlikely() intends for the cost of a branch misprediction to be paid
/// for fetching and executing this function. This is because we strive to
/// optimize the pipeline for the nominal path, making this device as cheap
/// as possible to use.
///
struct ircd::util::unwind::exceptional
{
	struct assertion;

	const std::function<void ()> func;

	template<class F>
	exceptional(F&& func)
	:func{std::forward<F>(func)}
	{}

	~exceptional() noexcept
	{
		if(unlikely(std::uncaught_exceptions()))
			func();
	}

	exceptional(const exceptional &) = delete;
};

/// Asserts that unwind is occurring without any exception. This allows some
/// section of code, for example, the latter half of a function, to assert that
/// it is not throwing and disrupting the other half. This is useful when no
/// exception is expected thus placing other guarantees should not be required
/// if this guarantee is met.
///
struct ircd::util::unwind::nominal::assertion
{
	~assertion() noexcept
	{
		assert(likely(!std::uncaught_exceptions()));
	}
};

/// Complements exceptional::assertion. This triggers an assertion when
/// unwinding WITHOUT an exception.
///
struct ircd::util::unwind::exceptional::assertion
{
	~assertion() noexcept
	{
		assert(likely(std::uncaught_exceptions()));
	}
};
