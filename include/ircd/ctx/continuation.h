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
	struct continuation;

	using yield_context = boost::asio::yield_context;
	using yield_closure = std::function<void (yield_context &)>;
	using interruptor = std::function<void (ctx *const &)>;
	using predicate = std::function<bool ()>;
}

namespace ircd
{
	using ctx::continuation;
}

/// This object must be placed on the stack when the context is yielding (INTERNAL)
///
/// The instance contains references to some callables which must remain valid.
///
/// - predicate (NOT YET USED)
/// A wakeup condition. This should be a simple boolean function which
/// tests whether the context should be woken up. The continuation references
/// this to convey the condition to a scheduler which may test many predicates
/// while contexts are asleep and then determine a schedule. This is an
/// alternative to waking up contexts first to test their predicates.
///
/// - interruptor
/// An interruption action. This is called when a context cannot wakeup on its
/// own after receiving an interruption without help from this action. Common
/// use for this is with yields to asio.
///
struct ircd::ctx::continuation
{
	static const predicate asio_predicate;
	static const predicate true_predicate;
	static const predicate false_predicate;
	static const interruptor noop_interruptor;

	ctx *const self;
	const predicate *const pred;
	const interruptor *const intr;

	operator const boost::asio::yield_context &() const noexcept;
	operator boost::asio::yield_context &() noexcept;

	continuation(const predicate &,
	             const interruptor &,
	             const yield_closure &);

	continuation(continuation &&) = delete;
	continuation(const continuation &) = delete;
	continuation &operator=(continuation &&) = delete;
	continuation &operator=(const continuation &) = delete;
	~continuation() noexcept;
};
