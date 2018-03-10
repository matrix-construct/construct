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

	template<class T> bool invalid(const shared_state<T> &);
	template<class T> bool pending(const shared_state<T> &);
	template<class T> bool ready(const shared_state<T> &);
	template<class T> void set_ready(shared_state<T> &);
	template<class T> void notify(shared_state<T> &);
}

struct ircd::ctx::shared_state_base
{
	mutable dock cond;
	std::exception_ptr eptr;
	std::function<void (shared_state_base &)> then;
};

template<class T>
struct ircd::ctx::shared_state
:shared_state_base
{
	using value_type      = T;
	using pointer_type    = T *;
	using reference_type  = T &;

	promise<T> *p {nullptr};
	T val;
};

template<>
struct ircd::ctx::shared_state<void>
:shared_state_base
{
	using value_type      = void;

	promise<void> *p {nullptr};
};

template<class T>
void
ircd::ctx::notify(shared_state<T> &st)
{
	st.cond.notify_all();

	if(!st.then)
		return;

	if(!current)
		return st.then(st);

	ircd::post([&st]
	{
		st.then(st);
	});
}

template<class T>
void
ircd::ctx::set_ready(shared_state<T> &st)
{
	st.p = reinterpret_cast<promise<T> *>(uintptr_t(0x42));
}

template<class T>
bool
ircd::ctx::ready(const shared_state<T> &st)
{
	return st.p == reinterpret_cast<const promise<T> *>(uintptr_t(0x42));
}

template<class T>
bool
ircd::ctx::pending(const shared_state<T> &st)
{
	return st.p > reinterpret_cast<const promise<T> *>(uintptr_t(0x42));
}

template<class T>
bool
ircd::ctx::invalid(const shared_state<T> &st)
{
	return st.p == nullptr;
}
