// Copyright (C) Matrix Construct Developers, Authors & Contributors
// Copyright (C) 2016-2018 Jason Volk
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies.
//
// THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
// IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
// WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
// DISCLAIMED. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT,
// INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
// (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
// SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
// HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
// STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
// IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
// POSSIBILITY OF SUCH DAMAGE.

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
	const std::function<void ()> func;

	template<class F>
	nominal(F&& func)
	:func{std::forward<F>(func)}
	{}

	~nominal() noexcept
	{
		if(likely(!std::uncaught_exception()))
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
	const std::function<void ()> func;

	template<class F>
	exceptional(F&& func)
	:func{std::forward<F>(func)}
	{}

	~exceptional() noexcept
	{
		if(unlikely(std::uncaught_exception()))
			func();
	}

	exceptional(const exceptional &) = delete;
};
