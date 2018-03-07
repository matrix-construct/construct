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
	IRCD_OVERLOAD(use_future)

	template<class T = void> class future;
	template<> class future<void>;
	template<class... T> struct scoped_future;

	enum class future_status;

	template<class T,
	         class time_point>
	future_status wait_until(const future<T> &, const time_point &);
}

enum class ircd::ctx::future_status
{
	ready,
	timeout,
	deferred,
};

template<class T>
class ircd::ctx::future
{
	std::shared_ptr<shared_state<T>> st;

  public:
	using value_type                             = typename shared_state<T>::value_type;
	using pointer_type                           = typename shared_state<T>::pointer_type;
	using reference_type                         = typename shared_state<T>::reference_type;

	bool valid() const                           { return bool(st);                                }
	bool operator!() const                       { return !valid();                                }
	operator bool() const                        { return valid();                                 }

	template<class U, class time_point> friend future_status wait_until(const future<U> &, const time_point &, std::nothrow_t);
	template<class U, class time_point> friend future_status wait_until(const future<U> &, const time_point &);
	template<class time_point> future_status wait_until(const time_point &, std::nothrow_t) const;
	template<class time_point> future_status wait_until(const time_point &) const;
	template<class duration> future_status wait(const duration &d, std::nothrow_t) const;
	template<class duration> future_status wait(const duration &d) const;
	void wait() const;

	T get();
	operator T()                                 { return get();                                   }

	void then(decltype(shared_state<T>::callback));

	future();
	future(promise<T> &promise);
};

template<>
class ircd::ctx::future<void>
{
	std::shared_ptr<shared_state<void>> st;

  public:
	using value_type                             = typename shared_state<void>::value_type;

	bool valid() const                           { return bool(st);                                }
	bool operator!() const                       { return !valid();                                }
	operator bool() const                        { return valid();                                 }

	template<class U, class time_point> friend future_status wait_until(const future<U> &, const time_point &, std::nothrow_t);
	template<class U, class time_point> friend future_status wait_until(const future<U> &, const time_point &);
	template<class time_point> future_status wait_until(const time_point &, std::nothrow_t) const;
	template<class time_point> future_status wait_until(const time_point &) const;
	template<class duration> future_status wait(const duration &d, std::nothrow_t) const;
	template<class duration> future_status wait(const duration &d) const;
	void wait() const;

	void then(decltype(shared_state<void>::callback));

	future();
	future(promise<void> &promise);
};

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
	if(std::uncaught_exception())
		return;

	if(this->valid())
		this->wait();
}

inline
ircd::ctx::future<void>::future()
:st{nullptr}
{
}

template<class T>
ircd::ctx::future<T>::future()
:st{nullptr}
{
}

inline
ircd::ctx::future<void>::future(promise<void> &promise)
:st{promise.get_state().share()}
{
}

template<class T>
ircd::ctx::future<T>::future(promise<T> &promise)
:st{promise.get_state().share()}
{
}

template<class T>
void
ircd::ctx::future<T>::then(decltype(shared_state<T>::callback) cb)
{
	assert(valid());
	st->callback = std::move(cb);
}

inline void
ircd::ctx::future<void>::then(decltype(shared_state<void>::callback) cb)
{
	assert(valid());
	st->callback = std::move(cb);
}

template<class T>
T
ircd::ctx::future<T>::get()
{
	wait();

	if(unlikely(bool(st->eptr)))
		std::rethrow_exception(st->eptr);

	return st->val;
}

inline void
ircd::ctx::future<void>::wait()
const
{
	this->wait_until(steady_clock::time_point::max());
}

template<class T>
void
ircd::ctx::future<T>::wait()
const
{
	this->wait_until(steady_clock::time_point::max());
}

template<class duration>
ircd::ctx::future_status
ircd::ctx::future<void>::wait(const duration &d)
const
{
	return this->wait_until(steady_clock::now() + d);
}

template<class duration>
ircd::ctx::future_status
ircd::ctx::future<void>::wait(const duration &d,
                              std::nothrow_t)
const
{
	return this->wait_until(steady_clock::now() + d, std::nothrow);
}

template<class T>
template<class duration>
ircd::ctx::future_status
ircd::ctx::future<T>::wait(const duration &d)
const
{
	return this->wait_until(steady_clock::now() + d);
}

template<class T>
template<class duration>
ircd::ctx::future_status
ircd::ctx::future<T>::wait(const duration &d,
                           std::nothrow_t)
const
{
	return this->wait_until(steady_clock::now() + d, std::nothrow);
}

template<class T>
template<class time_point>
ircd::ctx::future_status
ircd::ctx::future<T>::wait_until(const time_point &tp)
const
{
	return ircd::ctx::wait_until(*this, tp);
}

template<class T>
template<class time_point>
ircd::ctx::future_status
ircd::ctx::future<T>::wait_until(const time_point &tp,
                                 std::nothrow_t)
const
{
	return ircd::ctx::wait_until(*this, tp, std::nothrow);
}

template<class time_point>
ircd::ctx::future_status
ircd::ctx::future<void>::wait_until(const time_point &tp)
const
{
	return ircd::ctx::wait_until(*this, tp);
}

template<class time_point>
ircd::ctx::future_status
ircd::ctx::future<void>::wait_until(const time_point &tp,
                                    std::nothrow_t)
const
{
	return ircd::ctx::wait_until(*this, tp, std::nothrow);
}

template<class T,
         class time_point>
ircd::ctx::future_status
ircd::ctx::wait_until(const future<T> &f,
                      const time_point &tp)
{
	const auto ret
	{
		wait_until(f, tp, std::nothrow)
	};

	if(ret == future_status::timeout)
		throw timeout{};

	return ret;
}

template<class T,
         class time_point>
ircd::ctx::future_status
ircd::ctx::wait_until(const future<T> &f,
                      const time_point &tp,
                      std::nothrow_t)
{
	const auto wfun([&f]() -> bool
	{
		return f.st->finished;
	});

	if(unlikely(!f.valid()))
		throw no_state();

	if(unlikely(!f.st->cond.wait_until(tp, wfun)))
		return future_status::timeout;

	return likely(wfun())? future_status::ready:
	                       future_status::deferred;
}
