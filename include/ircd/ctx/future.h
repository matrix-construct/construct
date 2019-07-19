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
#define HAVE_IRCD_CTX_FUTURE_H

namespace ircd::ctx
{
	template<class T = void> class future;
	template<> class future<void>;

	template<class... T> struct scoped_future;

	IRCD_EXCEPTION(future_error, future_already_retrieved)
	IRCD_OVERLOAD(use_future)

	template<class T> const shared_state<T> &state(const future<T> &);
	template<class T> shared_state<T> &state(future<T> &);

	template<class T,
	         class time_point>
	bool _wait_until(const future<T> &, const time_point &, std::nothrow_t);

	template<class T,
	         class time_point>
	void _wait_until(const future<T> &, const time_point &);
}

template<class T>
struct ircd::ctx::future
:private shared_state<T>
{
	using value_type                             = typename shared_state<T>::value_type;
	using pointer_type                           = typename shared_state<T>::pointer_type;
	using reference_type                         = typename shared_state<T>::reference_type;

	const shared_state<T> &state() const         { return *this;                                   }
	shared_state<T> &state()                     { return *this;                                   }

	bool valid() const                           { return !is(state(), future_state::INVALID);     }
	bool operator!() const                       { return !valid();                                }
	operator bool() const                        { return valid();                                 }

	template<class U, class time_point> friend bool wait_until(const future<U> &, const time_point &, std::nothrow_t);
	template<class U, class time_point> friend void wait_until(const future<U> &, const time_point &);
	template<class time_point> bool wait_until(const time_point &, std::nothrow_t) const;
	template<class time_point> void wait_until(const time_point &) const;
	template<class duration> bool wait(const duration &d, std::nothrow_t) const;
	template<class duration> void wait(const duration &d) const;
	void wait() const;

	T get();
	operator T()                                 { return get();                                   }

	future() = default;
	future(promise<T> &promise);
	future(future &&) noexcept;
	future(const future &) = delete;
	future &operator=(future &&) noexcept;
	future &operator=(const future &) = delete;
	~future() noexcept;
};

template<>
struct ircd::ctx::future<void>
:private shared_state<void>
{
	using value_type                             = typename shared_state<void>::value_type;

	const shared_state<void> &state() const      { return *this;                                   }
	shared_state<void> &state()                  { return *this;                                   }

	bool valid() const                           { return !is(state(), future_state::INVALID);     }
	bool operator!() const                       { return !valid();                                }
	operator bool() const                        { return valid();                                 }

	template<class U, class time_point> friend bool wait_until(const future<U> &, const time_point &, std::nothrow_t);
	template<class U, class time_point> friend void wait_until(const future<U> &, const time_point &);
	template<class time_point> bool wait_until(const time_point &, std::nothrow_t) const;
	template<class time_point> void wait_until(const time_point &) const;
	template<class duration> bool wait(const duration &d, std::nothrow_t) const;
	template<class duration> void wait(const duration &d) const;
	void wait() const;

	IRCD_OVERLOAD(already)

	future(promise<void> &promise);
	future(already_t);                           // construct in ready state
	future() = default;
	future(future &&) noexcept;
	future(const future &) = delete;
	future &operator=(future &&) noexcept;
	future &operator=(const future &) = delete;
	~future() noexcept;
};

namespace ircd::ctx
{
}

template<class... T>
struct ircd::ctx::scoped_future
:future<T...>
{
	template<class... Args> scoped_future(Args&&... args);
	~scoped_future() noexcept;
};

template<class... T>
template<class... Args>
ircd::ctx::scoped_future<T...>::scoped_future(Args&&... args)
:future<T...>{std::forward<Args>(args)...}
{
}

template<class... T>
ircd::ctx::scoped_future<T...>::~scoped_future()
noexcept
{
	if(std::uncaught_exceptions())
		return;

	if(this->valid())
		this->wait();
}

template<class T>
ircd::ctx::future<T>::future(promise<T> &promise)
:shared_state<T>{promise}
{
	assert(!promise.valid());
	update(state());
	assert(promise.valid());
}

inline
ircd::ctx::future<void>::future(promise<void> &promise)
:shared_state<void>{promise}
{
	assert(!promise.valid());
	update(state());
	assert(promise.valid());
}

inline
ircd::ctx::future<void>::future(already_t)
{
	set(state(), future_state::READY);
}

template<class T>
ircd::ctx::future<T>::future(future<T> &&o)
noexcept
:shared_state<T>{std::move(o)}
{
	update(state());
	o.state().p = nullptr;
}

inline
ircd::ctx::future<void>::future(future<void> &&o)
noexcept
:shared_state<void>{std::move(o)}
{
	update(state());
	o.state().p = nullptr;
}

template<class T>
ircd::ctx::future<T> &
ircd::ctx::future<T>::operator=(future<T> &&o)
noexcept
{
	this->~future();
	static_cast<shared_state<T> &>(*this) = std::move(o);
	update(state());
	o.state().p = nullptr;
	return *this;
}

inline ircd::ctx::future<void> &
ircd::ctx::future<void>::operator=(future<void> &&o)
noexcept
{
	this->~future();
	static_cast<shared_state<void> &>(*this) = std::move(o);
	update(state());
	o.state().p = nullptr;
	return *this;
}

template<class T>
ircd::ctx::future<T>::~future()
noexcept
{
	invalidate(state());
}

inline
ircd::ctx::future<void>::~future()
noexcept
{
	invalidate(state());
}

template<class T>
T
ircd::ctx::future<T>::get()
{
	wait();
	if(unlikely(is(state(), future_state::RETRIEVED)))
		throw future_already_retrieved{};

	set(state(), future_state::RETRIEVED);
	if(bool(state().eptr))
		std::rethrow_exception(state().eptr);

	return state().val;
}

template<class T>
void
ircd::ctx::future<T>::wait()
const
{
	this->wait_until(steady_clock::time_point::max());
}

inline void
ircd::ctx::future<void>::wait()
const
{
	this->wait_until(steady_clock::time_point::max());
}

template<class T>
template<class duration>
void
ircd::ctx::future<T>::wait(const duration &d)
const
{
	this->wait_until(steady_clock::now() + d);
}

template<class duration>
void
ircd::ctx::future<void>::wait(const duration &d)
const
{
	this->wait_until(steady_clock::now() + d);
}

template<class T>
template<class duration>
bool
ircd::ctx::future<T>::wait(const duration &d,
                           std::nothrow_t)
const
{
	return this->wait_until(steady_clock::now() + d, std::nothrow);
}

template<class duration>
bool
ircd::ctx::future<void>::wait(const duration &d,
                              std::nothrow_t)
const
{
	return this->wait_until(steady_clock::now() + d, std::nothrow);
}

template<class T>
template<class time_point>
void
ircd::ctx::future<T>::wait_until(const time_point &tp)
const
{
	_wait_until(*this, tp);
}

template<class time_point>
void
ircd::ctx::future<void>::wait_until(const time_point &tp)
const
{
	if(!this->wait_until(tp, std::nothrow))
		throw timeout{};
}

template<class T>
template<class time_point>
bool
ircd::ctx::future<T>::wait_until(const time_point &tp,
                                 std::nothrow_t)
const
{
	return _wait_until(*this, tp, std::nothrow);
}

template<class time_point>
bool
ircd::ctx::future<void>::wait_until(const time_point &tp,
                                    std::nothrow_t)
const
{
	if(_wait_until(*this, tp, std::nothrow))
	{
		auto &state
		{
			const_cast<future<void> *>(this)->state()
		};

		set(state, future_state::RETRIEVED);
		if(bool(state.eptr))
			std::rethrow_exception(state.eptr);

		return true;
	}
	else return false;
}

template<class T,
         class time_point>
void
ircd::ctx::wait_until(const future<T> &f,
                      const time_point &tp)
{
	if(!_wait_until(f, tp, std::nothrow))
		throw timeout{};
}

template<class T,
         class time_point>
bool
ircd::ctx::_wait_until(const future<T> &f,
                       const time_point &tp,
                       std::nothrow_t)
{
	auto &state
	{
		const_cast<future<T> &>(f).state()
	};

	if(unlikely(is(state, future_state::INVALID)))
		throw no_state{};

	return state.cond.wait_until(tp, [&state]
	{
		return !is(state, future_state::PENDING);
	});
}

template<class T>
ircd::ctx::shared_state<T> &
ircd::ctx::state(future<T> &future)
{
	return future.state();
}

template<class T>
const ircd::ctx::shared_state<T> &
ircd::ctx::state(const future<T> &future)
{
	return future.state();
}
