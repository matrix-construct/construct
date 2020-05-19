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

namespace ircd::ctx
{
	bool interruptible(const ctx &) noexcept;
	void interruptible(ctx &, const bool &) noexcept;
}

namespace ircd::ctx {
inline namespace this_ctx
{
	struct uninterruptible;

	bool interruptible() noexcept;
	void interruptible(const bool &);
	void interruptible(const bool &, std::nothrow_t) noexcept;
}}

/// An instance of uninterruptible will suppress interrupts sent to the
/// context for the scope. Suppression does not discard any interrupt,
/// it merely ignores it at all interruption points until the suppression
/// ends, after which it will be thrown.
///
struct ircd::ctx::this_ctx::uninterruptible
{
	struct nothrow;

	bool theirs;

	uninterruptible(const bool & = true);
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

	nothrow(const bool & = true) noexcept;
	nothrow(nothrow &&) = delete;
	nothrow(const nothrow &) = delete;
	~nothrow() noexcept;
};

//
// uninterruptible
//

inline
ircd::ctx::this_ctx::uninterruptible::uninterruptible(const bool &ours)
:theirs
{
	interruptible(cur())
}
{
	interruptible(!ours);
}

inline
ircd::ctx::this_ctx::uninterruptible::~uninterruptible()
noexcept(false)
{
	interruptible(theirs);
}

//
// uninterruptible::nothrow
//

inline
ircd::ctx::this_ctx::uninterruptible::nothrow::nothrow(const bool &ours)
noexcept
:theirs
{
	interruptible(cur())
}
{
	interruptible(!ours, std::nothrow);
}

inline
ircd::ctx::this_ctx::uninterruptible::nothrow::~nothrow()
noexcept
{
	interruptible(theirs, std::nothrow);
}

//
// interruptible
//

inline void
ircd::ctx::this_ctx::interruptible(const bool &b,
                                   std::nothrow_t)
noexcept
{
	interruptible(cur(), b);
}

inline bool
ircd::ctx::this_ctx::interruptible()
noexcept
{
	return interruptible(cur());
}

/// Marks `ctx` for whether to allow or suppress interruption. Suppression
/// does not ignore an interrupt itself, it only ignores the interruption
/// points. Thus when a suppression ends if the interrupt flag was ever set
/// the next interruption point will throw as expected.
inline void
ircd::ctx::interruptible(ctx &ctx,
                         const bool &b)
noexcept
{
	flags(ctx) ^= (flags(ctx) ^ (ulong(b) - 1)) & context::NOINTERRUPT;
	assert(bool(flags(ctx) & context::NOINTERRUPT) == !b);
	assert(interruptible(ctx) == b);
}

inline bool
ircd::ctx::interruptible(const ctx &ctx)
noexcept
{
	return ~flags(ctx) & context::NOINTERRUPT;
}
