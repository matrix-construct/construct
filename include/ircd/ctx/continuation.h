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
#define HAVE_IRCD_CTX_CONTINUATION_H

// This file is not included with the IRCd standard include stack because
// it requires symbols we can't forward declare without boost headers. It
// is part of the <ircd/asio.h> stack which can be included in your
// definition file if necessary.

namespace ircd::ctx
{
	using yield_context = boost::asio::yield_context;

	struct continuation;
	struct to_asio;
}

namespace ircd
{
	using ctx::yield_context;
	using ctx::continuation;
	using ctx::to_asio;
}

/// This object must be placed on the stack when the context is yielding (INTERNAL)
///
/// The continuation constructor is the last thing executed before a context
/// yields. The continuation destructor is the first thing executed when a
/// context continues. This is not placed by a normal user wishing to context
/// switch, only a low-level library creator actually implementing the context
/// switch itself. The placement of this object must be correct. Generally,
/// we construct the `continuation` as an argument to `yield_context` as such
/// `yield_context{continuation{}}`. This ensures the continuation destructor
/// executes before control continues beyond the yield_context call itself and
/// ties this sequence together neatly.
///
struct ircd::ctx::continuation
{
	ctx *self;

	operator const boost::asio::yield_context &() const;
	operator boost::asio::yield_context &();

	virtual void interrupted(ctx *const &) noexcept;

	continuation();
	virtual ~continuation() noexcept;
};

/// This type of continuation should be used when yielding a context to a
/// boost::asio handler so we can have specific control over that type of
/// context switch in possible contrast or extension of the regular
/// continuation behavior.
///
/// The statement `yield_context{to_asio{}}` can be passed to any boost::asio
/// callback handler. Those handlers have overloads to accept this, many of
/// which are not documented.
///
struct ircd::ctx::to_asio
:ircd::ctx::continuation
{
	using function = std::function<void (ctx *const &)>;

	function handler;

	void interrupted(ctx *const &) noexcept final override;

	to_asio(const function &handler = {});
};

inline
ircd::ctx::to_asio::to_asio(const function &handler)
:handler{handler}
{}
