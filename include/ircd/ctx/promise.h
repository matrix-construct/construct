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
/// allocating the promise in a shared_ptr and refcounting... Note that the
/// same semantic exists on the future side to implement shared futures. Both
/// parties maintain a pointer to the head of a singly linked list of the
/// other party, and a pointer to the next instance of their own party.
struct ircd::ctx::promise_base
{
	static const promise_base *head(const shared_state_base &);
	static const promise_base *head(const promise_base &);
	static size_t refcount(const shared_state_base &);
	static size_t refcount(const promise_base &);

	static promise_base *head(shared_state_base &);
	static promise_base *head(promise_base &);

	shared_state_base *st {nullptr};      // the head of all sharing futures
	promise_base *next {nullptr};         // next sharing promise

	template<class T> const shared_state<T> &state() const noexcept;
	template<class T> shared_state<T> &state() noexcept;
	const shared_state_base &state() const noexcept;
	shared_state_base &state() noexcept;

	void check_pending() const;
	void make_ready();
	void remove();

  public:
	bool valid() const noexcept;
	bool operator!() const noexcept;
	explicit operator bool() const noexcept;

	void set_exception(std::exception_ptr);

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
inline void
ircd::ctx::promise<T>::set_value(T&& val)
{
	if(!valid())
		return;

	check_pending();
	auto *state
	{
		shared_state_base::head(*this)
	};

	assert(state);
	if(shared_state_base::refcount(*state) > 1) do
	{
		assert(is(*state, future_state::PENDING));
		static_cast<shared_state<T> &>(*state).val = val;
	}
	while((state = state->next)); else
	{
		assert(is(this->state(), future_state::PENDING));
		this->state().val = std::move(val);
	}

	make_ready();
}

template<class T>
inline void
ircd::ctx::promise<T>::set_value(const T &val)
{
	if(!valid())
		return;

	check_pending();
	auto *state(shared_state_base::head(*this)); do
	{
		assert(state);
		assert(is(*state, future_state::PENDING));
		static_cast<shared_state<T> &>(*state).val = val;
	}
	while((state = state->next));
	make_ready();
}

template<class T>
inline ircd::ctx::shared_state<T> &
ircd::ctx::promise<T>::state()
{
	return promise_base::state<T>();
}

template<class T>
inline const ircd::ctx::shared_state<T> &
ircd::ctx::promise<T>::state()
const
{
	return promise_base::state<T>();
}

//
// promise_base
//

inline void
ircd::ctx::promise_base::check_pending()
const
{
	assert(valid());
	if(unlikely(!is(state(), future_state::PENDING)))
		throw promise_already_satisfied{};
}

inline bool
ircd::ctx::promise_base::operator!()
const noexcept
{
	return !valid();
}

inline ircd::ctx::promise_base::operator
bool()
const noexcept
{
	return valid();
}

inline bool
ircd::ctx::promise_base::valid()
const noexcept
{
	return bool(st);
}

template<class T>
inline ircd::ctx::shared_state<T> &
ircd::ctx::promise_base::state()
noexcept
{
	return static_cast<shared_state<T> &>(state());
}

template<class T>
inline const ircd::ctx::shared_state<T> &
ircd::ctx::promise_base::state()
const noexcept
{
	return static_cast<const shared_state<T> &>(state());
}

inline ircd::ctx::shared_state_base &
ircd::ctx::promise_base::state()
noexcept
{
	assert(valid());
	return *st;
}

inline const ircd::ctx::shared_state_base &
ircd::ctx::promise_base::state()
const noexcept
{
	assert(valid());
	return *st;
}
