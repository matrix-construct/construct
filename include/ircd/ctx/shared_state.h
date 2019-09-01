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
#define HAVE_IRCD_CTX_SHARED_STATE_H

namespace ircd::ctx
{
	enum class future_state :uintptr_t;
	struct shared_state_base;
	struct promise_base;

	template<class T> struct shared_state;
	template<> struct shared_state<void>;

	IRCD_EXCEPTION(ircd::ctx::error, future_error)

	future_state state(const shared_state_base &);
	bool is(const shared_state_base &, const future_state &);
	void set(shared_state_base &, const future_state &);
	size_t refcount(const shared_state_base &);
	void invalidate(shared_state_base &);
	void update(shared_state_base &);
	void notify(shared_state_base &);
}

/// Internal state enumeration for the promise / future / related. These can
/// all be observed through state() or is(); only some can be set(). This is
/// not for public manipulation.
enum class ircd::ctx::future_state
:uintptr_t
{
	INVALID    = 0,  ///< Null.
	PENDING    = 1,  ///< Promise is attached and busy.
	READY      = 2,  ///< Result ready; promise is gone.
	OBSERVED   = 3,  ///< Special case for when_*(); not a state; promise is gone.
	RETRIEVED  = 4,  ///< User retrieved future value; promise is gone.
};

/// Internal Non-template base of the state object shared by promise and
/// future. It is extended by the appropriate template, and usually resides
/// in the future's instance, where the promise finds it.
struct ircd::ctx::shared_state_base
{
	mutable dock cond;
	std::exception_ptr eptr;
	std::function<void (shared_state_base &)> then;
	union
	{
		promise_base *p {nullptr};
		future_state st;
	};

	shared_state_base(promise_base &p);
	shared_state_base() = default;
	shared_state_base(shared_state_base &&) = default;
	shared_state_base(const shared_state_base &) = delete;
	shared_state_base &operator=(shared_state_base &&) = default;
	shared_state_base &operator=(const shared_state_base &) = delete;
	~shared_state_base() noexcept;
};

/// Internal shared state between future and promise appropos a future value.
template<class T>
struct ircd::ctx::shared_state
:shared_state_base
{
	using value_type = T;
	using pointer_type = T *;
	using reference_type = T &;

	T val;

	using shared_state_base::shared_state_base;
	using shared_state_base::operator=;
};

/// Internal shared state between future and promise when there is no future
/// value, only a notification of completion or exception.
template<>
struct ircd::ctx::shared_state<void>
:shared_state_base
{
	using value_type = void;

	using shared_state_base::shared_state_base;
	using shared_state_base::operator=;
};
