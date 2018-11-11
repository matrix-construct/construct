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
#define HAVE_IRCD_CTX_UNINTERRUPTIBLE_H

namespace ircd::ctx::this_ctx
{
	struct uninterruptible;

	bool interruptible() noexcept;
	void interruptible(const bool &);
	void interruptible(const bool &, std::nothrow_t) noexcept;
}

/// An instance of uninterruptible will suppress interrupts sent to the
/// context for the scope. Suppression does not discard any interrupt,
/// it merely ignores it at all interruption points until the suppression
/// ends, after which it will be thrown.
///
struct ircd::ctx::this_ctx::uninterruptible
{
	struct nothrow;

	bool theirs;

	uninterruptible();
	uninterruptible(uninterruptible &&) = delete;
	uninterruptible(const uninterruptible &) = delete;
	~uninterruptible() noexcept(false);
};

/// A variant of uinterruptible for users that must guarantee the ending of
/// the suppression scope will not be an interruption point. The default
/// behavior for uninterruptible is to throw, even from its destructor, to
/// fulfill the interruption request without any more delay.
///
struct ircd::ctx::this_ctx::uninterruptible::nothrow
{
	bool theirs;

	nothrow() noexcept;
	nothrow(nothrow &&) = delete;
	nothrow(const nothrow &) = delete;
	~nothrow() noexcept;
};
