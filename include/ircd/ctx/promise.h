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
#define HAVE_IRCD_CTX_PROMISE_H

namespace ircd::ctx
{
	struct promise_base;
	template<class T = void> class promise;
	template<> class promise<void>;

	IRCD_EXCEPTION(future_error, no_state)
	IRCD_EXCEPTION(future_error, broken_promise)
	IRCD_EXCEPTION(future_error, promise_already_satisfied)
}

/// Abstract base type for ctx::promise. This dedupes most of the promissory
/// functionality with non-template definitions residing in ctx.cc.
///
/// In this system the promise is lightweight and maintains a pointer to the
/// shared_state object which generally resides within the future instance.
/// If the shared_state object moves or is destroyed the promise's pointer to
/// it must be updated. The shared_state object also has a pointer to the
/// promise; if the promise moves or is destroyed that pointer must be updated
/// as well. This is how the bi-directional relationship is maintained.
///
/// To further complicate things, this promise maintains a second pointer
/// to another instance of a promise implementing a linked-list semantic. All
/// of these promises are making the same promise to the same shared_state;
/// the list allows for copy semantics which are important for some callback
/// systems (like boost::asio). This solution is far more optimal than
/// allocating the promise in a shared_ptr and refcounting...
struct ircd::ctx::promise_base
{
	static void remove(shared_state_base &, promise_base &);
	static void update(promise_base &new_, promise_base &old);
	static void append(promise_base &new_, promise_base &old);

	shared_state_base *st {nullptr};
	mutable promise_base *next {nullptr};

	template<class T> const shared_state<T> &state() const;
	template<class T> shared_state<T> &state();
	const shared_state_base &state() const;
	shared_state_base &state();

	void check_pending() const;
	void make_ready();

  public:
	bool valid() const;
	operator bool() const;
	bool operator!() const;

	void set_exception(std::exception_ptr eptr);

  protected:
	promise_base() = default;
	promise_base(const promise_base &);
	promise_base(promise_base &&) noexcept;
	promise_base &operator=(const promise_base &);
	promise_base &operator=(promise_base &&) noexcept;
	~promise_base() noexcept;
};

/// Value-oriented promise. The user creates an instance of this promise in
/// order to send a value to a future. After creating an instance of this
/// promise the user should construct a future with the matching template type
/// from this. The two will then be linked.
///
/// Space for the value will reside within the future instance and not the
/// promise instance. When the value is set it will be copied (or movied) there.
///
/// Full object semantics for this promise are supported; including copy
/// semantics. All copies of a promise are making the same promise thus
/// setting a value or exception for one invalidates all the copies.
///
/// Instances of this promise can safely destruct at any time. When all copies
/// of a promise destruct without setting a value or exception the future is
/// notified with a broken_promise exception.
template<class T>
struct ircd::ctx::promise
:promise_base
{
	using value_type = typename shared_state<T>::value_type;
	using pointer_type = typename shared_state<T>::pointer_type;
	using reference_type = typename shared_state<T>::reference_type;

	const shared_state<T> &state() const;
	shared_state<T> &state();

  public:
	void set_value(const T &val);
	void set_value(T&& val);

	using promise_base::promise_base;
	using promise_base::operator=;
};

/// Valueless promise. The user creates an instance of this promise in
/// order to notify a future of a success or set an exception for failure.
///
/// See docs for promise<T> for more information.
template<>
struct ircd::ctx::promise<void>
:promise_base
{
	using value_type = typename shared_state<void>::value_type;

	const shared_state<void> &state() const;
	shared_state<void> &state();

  public:
	void set_value();

	using promise_base::promise_base;
	using promise_base::operator=;
};

//
// promise<T>
//

template<class T>
void
ircd::ctx::promise<T>::set_value(T&& val)
{
	check_pending();
	state().val = std::move(val);
	make_ready();
}

template<class T>
void
ircd::ctx::promise<T>::set_value(const T &val)
{
	check_pending();
	state().val = val;
	make_ready();
}

template<class T>
ircd::ctx::shared_state<T> &
ircd::ctx::promise<T>::state()
{
	return promise_base::state<T>();
}

template<class T>
const ircd::ctx::shared_state<T> &
ircd::ctx::promise<T>::state()
const
{
	return promise_base::state<T>();
}

//
// promise_base
//

template<class T>
ircd::ctx::shared_state<T> &
ircd::ctx::promise_base::state()
{
	return static_cast<shared_state<T> &>(state());
}

template<class T>
const ircd::ctx::shared_state<T> &
ircd::ctx::promise_base::state()
const
{
	return static_cast<const shared_state<T> &>(state());
}
