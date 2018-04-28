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
	struct shared_state_base;
	template<class T = void> struct shared_state;
	template<> struct shared_state<void>;
	template<class T> struct promise;
	template<> struct promise<void>;
	enum class future_state;

	template<class T> future_state state(const shared_state<T> &);
	template<class T> bool is(const shared_state<T> &, const future_state &);
	template<class T> void set(shared_state<T> &, const future_state &);
	template<> void set(shared_state<void> &, const future_state &);
	template<class T> void notify(shared_state<T> &);
}

/// Internal state enumeration for the promise / future / related. These can
/// all be observed through state() or is(); only some can be set(). This is
/// not for public manipulation.
enum class ircd::ctx::future_state
{
	INVALID,     ///< Null.
	PENDING,     ///< Promise is attached and busy.
	READY,       ///< Result ready; promise is gone.
	OBSERVED,    ///< Special case for when_*(); not a state; promise is gone.
	RETRIEVED,   ///< User retrieved future value; promise is gone.
};

/// Internal Non-template base of the state object shared by promise and
/// future. It is extended by the appropriate template, and usually resides
/// in the future's instance, where the promise finds it.
struct ircd::ctx::shared_state_base
{
	mutable dock cond;
	std::exception_ptr eptr;
	std::function<void (shared_state_base &)> then;
};

/// Internal shared state between future and promise appropos a future value.
template<class T>
struct ircd::ctx::shared_state
:shared_state_base
{
	using value_type      = T;
	using pointer_type    = T *;
	using reference_type  = T &;

	promise<T> *p {nullptr};
	T val;

	shared_state(promise<T> &p)
	:p{&p}
	{}

	shared_state() = default;
};

/// Internal shared state between future and promise when there is no future
/// value, only a notification of completion or exception.
template<>
struct ircd::ctx::shared_state<void>
:shared_state_base
{
	using value_type      = void;

	promise<void> *p {nullptr};

	shared_state(promise<void> &p)
	:p{&p}
	{}

	shared_state() = default;
};

/// Internal use
template<class T>
void
ircd::ctx::notify(shared_state<T> &st)
{
	if(!st.then)
	{
		st.cond.notify_all();
		return;
	}

	if(!current)
	{
		st.cond.notify_all();
		assert(bool(st.then));
		st.then(st);
		return;
	}

	const stack_usage_assertion sua;
	st.cond.notify_all();
	assert(bool(st.then));
	st.then(st);
}

/// Internal use
template<class T>
void
ircd::ctx::set(shared_state<T> &st,
               const future_state &state)
{
	#pragma GCC diagnostic push
	#pragma GCC diagnostic ignored "-Wstrict-aliasing"
	switch(state)
	{
		case future_state::INVALID:  assert(0);  return;
		case future_state::PENDING:  assert(0);  return;
		case future_state::OBSERVED:
		case future_state::READY:
			reinterpret_cast<uintptr_t &>(st.p) = uintptr_t(0x42);
			return;

		case future_state::RETRIEVED:
			reinterpret_cast<uintptr_t &>(st.p) = uintptr_t(0x123);
			return;
	}
	#pragma GCC diagnostic pop
}

/// Internal use
template<>
inline void
ircd::ctx::set(shared_state<void> &st,
               const future_state &state)
{
	#pragma GCC diagnostic push
	#pragma GCC diagnostic ignored "-Wstrict-aliasing"
	switch(state)
	{
		case future_state::INVALID:  assert(0);  return;
		case future_state::PENDING:  assert(0);  return;
		case future_state::READY:
			reinterpret_cast<uintptr_t &>(st.p) = uintptr_t(0x42);
			return;

		case future_state::OBSERVED:
		case future_state::RETRIEVED:
			reinterpret_cast<uintptr_t &>(st.p) = uintptr_t(0x123);
			return;
	}
	#pragma GCC diagnostic pop
}

/// Internal; check if the current state is something; safe but unnecessary
/// for public use.
template<class T>
bool
ircd::ctx::is(const shared_state<T> &st,
              const future_state &state_)
{
	return state(st) == state_;
}

/// Internal; get the current state of the shared_state; safe but unnecessary
/// for public use.
template<class T>
ircd::ctx::future_state
ircd::ctx::state(const shared_state<T> &st)
{
	switch(uintptr_t(st.p))
	{
		case 0x00:       return future_state::INVALID;
		case 0x42:       return future_state::READY;
		case 0x123:      return future_state::RETRIEVED;
		default:
			assert(uintptr_t(st.p) >= 0x1000);
			return future_state::PENDING;
	}
}
