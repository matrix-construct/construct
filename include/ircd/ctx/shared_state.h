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
	IRCD_OVERLOAD(already)

	future_state state(const shared_state_base &) noexcept;
	bool is(const shared_state_base &, const future_state &) noexcept;
	void set(shared_state_base &, const future_state &);
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
///
/// There can be multiple promises and multiple futures all associated with
/// the same resolution event. All promises point to the first
/// shared_state_base (future) of the associated shared_state_base list. When
/// any promise in the list of associated promises sets a result, it copies
/// the result to all futures in the list; if only one future, it std::move()'s
/// the result; then the association of all promises and all futures and
/// respective lists are invalidated.
///
/// Note that the only way to traverse the list of shared_state_bases's is to
/// dereference the promise pointer (head promise) and follow the st->next
/// list. The only way to traverse the list of promises is to dereference a
/// shared_state_base with a valid *p in future_state::PENDING and chase the
/// p->next list. Each side of the system relies on the other. This means any
/// proper iteration of the promise or future lists can only take place before
/// dissolution of the system.
struct ircd::ctx::shared_state_base
{
	static const shared_state_base *head(const shared_state_base &);
	static const shared_state_base *head(const promise_base &);
	static size_t refcount(const shared_state_base &);

	static shared_state_base *head(shared_state_base &);
	static shared_state_base *head(promise_base &);

	mutable dock cond;
	std::exception_ptr eptr;
	std::function<void (shared_state_base &)> then;
	shared_state_base *next{nullptr}; // next sharing future
	union alignas(8)
	{
		promise_base *p; // the head of all sharing promises
		future_state st {future_state::INVALID};
	};

	shared_state_base() noexcept;
	shared_state_base(already_t) noexcept;
	shared_state_base(promise_base &);
	shared_state_base(shared_state_base &&) noexcept;
	shared_state_base(const shared_state_base &);
	shared_state_base &operator=(promise_base &);
	shared_state_base &operator=(shared_state_base &&) noexcept;
	shared_state_base &operator=(const shared_state_base &);
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

	template<class U> shared_state(already_t, U&&);
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

template<class T>
template<class U>
inline
ircd::ctx::shared_state<T>::shared_state(already_t,
                                         U&& val)
:shared_state_base
{
	already
}
,val
{
	std::forward<U>(val)
}
{}
