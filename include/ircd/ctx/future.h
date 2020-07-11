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
	explicit operator T()                        { return get();                                   }

	template<class U, class time_point> friend bool wait_until(const future<U> &, const time_point &, std::nothrow_t);
	template<class U, class time_point> friend void wait_until(const future<U> &, const time_point &);
	template<class time_point> bool wait_until(const time_point &, std::nothrow_t) const;
	template<class time_point> void wait_until(const time_point &) const;
	template<class duration> bool wait(const duration &d, std::nothrow_t) const;
	template<class duration> void wait(const duration &d) const;
	void wait() const;

	T get();
	template<class duration> T get(const duration &d);
	template<class time_point> T get_until(const time_point &);

	using shared_state<T>::shared_state;
	using shared_state<T>::operator=;
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

	template<class U, class time_point> friend bool wait_until(const future<U> &, const time_point &, std::nothrow_t);
	template<class U, class time_point> friend void wait_until(const future<U> &, const time_point &);
	template<class time_point> bool wait_until(const time_point &, std::nothrow_t) const;
	template<class time_point> void wait_until(const time_point &) const;
	template<class duration> bool wait(const duration &d, std::nothrow_t) const;
	template<class duration> void wait(const duration &d) const;
	void wait() const;

	using shared_state<void>::shared_state;
	using shared_state<void>::operator=;
};

template<class... T>
struct ircd::ctx::scoped_future
:future<T...>
{
	using future<T...>::future;
	~scoped_future() noexcept;
};

template<class... T>
inline
ircd::ctx::scoped_future<T...>::~scoped_future()
noexcept
{
	if(std::uncaught_exceptions() || !this->valid())
		return;

	this->wait();
}

template<class T>
template<class time_point>
inline T
ircd::ctx::future<T>::get_until(const time_point &tp)
{
	this->wait_until(tp);
	return this->get();
}

template<class T>
template<class duration>
inline T
ircd::ctx::future<T>::get(const duration &d)
{
	this->wait(d);
	return this->get();
}

template<class T>
inline T
ircd::ctx::future<T>::get()
{
	wait();
	if(unlikely(is(state(), future_state::RETRIEVED)))
		throw future_already_retrieved{};

	set(state(), future_state::RETRIEVED);
	if(bool(state().eptr))
		std::rethrow_exception(state().eptr);

	return std::move(state().val);
}

template<class T>
inline void
ircd::ctx::future<T>::wait()
const
{
	this->wait_until(system_point::max());
}

inline void
ircd::ctx::future<void>::wait()
const
{
	this->wait_until(system_point::max());
}

template<class T>
template<class duration>
inline void
ircd::ctx::future<T>::wait(const duration &d)
const
{
	this->wait_until(now<system_point>() + d);
}

template<class duration>
inline void
ircd::ctx::future<void>::wait(const duration &d)
const
{
	this->wait_until(now<system_point>() + d);
}

template<class T>
template<class duration>
inline bool
ircd::ctx::future<T>::wait(const duration &d,
                           std::nothrow_t)
const
{
	return this->wait_until(now<system_point>() + d, std::nothrow);
}

template<class duration>
inline bool
ircd::ctx::future<void>::wait(const duration &d,
                              std::nothrow_t)
const
{
	return this->wait_until(now<system_point>() + d, std::nothrow);
}

template<class T>
template<class time_point>
inline void
ircd::ctx::future<T>::wait_until(const time_point &tp)
const
{
	if(!this->wait_until(tp, std::nothrow))
		throw timeout{};
}

template<class time_point>
inline void
ircd::ctx::future<void>::wait_until(const time_point &tp)
const
{
	if(!this->wait_until(tp, std::nothrow))
		throw timeout{};
}

template<class T>
template<class time_point>
inline bool
ircd::ctx::future<T>::wait_until(const time_point &tp,
                                 std::nothrow_t)
const
{
	return _wait_until(*this, tp, std::nothrow);
}

template<class time_point>
inline bool
ircd::ctx::future<void>::wait_until(const time_point &tp,
                                    std::nothrow_t)
const
{
	if(_wait_until(*this, tp, std::nothrow))
	{
		auto &state(mutable_cast(this->state()));
		set(state, future_state::RETRIEVED);
		if(bool(state.eptr))
			std::rethrow_exception(state.eptr);

		return true;
	}
	else return false;
}

template<class T,
         class time_point>
inline void
ircd::ctx::wait_until(const future<T> &f,
                      const time_point &tp)
{
	if(!_wait_until(f, tp, std::nothrow))
		throw timeout{};
}

template<class T,
         class time_point>
inline bool
ircd::ctx::_wait_until(const future<T> &f,
                       const time_point &tp,
                       std::nothrow_t)
{
	auto &state(mutable_cast(f.state()));
	if(unlikely(is(state, future_state::INVALID)))
		throw no_state{};

	return state.cond.wait_until(tp, [&state]() noexcept
	{
		return !is(state, future_state::PENDING);
	});
}

template<class T>
inline ircd::ctx::shared_state<T> &
ircd::ctx::state(future<T> &future)
{
	return future.state();
}

template<class T>
inline const ircd::ctx::shared_state<T> &
ircd::ctx::state(const future<T> &future)
{
	return future.state();
}
