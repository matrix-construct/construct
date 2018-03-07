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
	IRCD_EXCEPTION(ircd::ctx::error, future_error)
	IRCD_EXCEPTION(future_error, no_state)
	IRCD_EXCEPTION(future_error, broken_promise)
	IRCD_EXCEPTION(future_error, future_already_retrieved)
	IRCD_EXCEPTION(future_error, promise_already_satisfied)

	template<class T = void> class promise;
	template<> class promise<void>;
}

template<class T>
class ircd::ctx::promise
{
	std::shared_ptr<shared_state<T>> st;

  public:
	using value_type                             = typename shared_state<T>::value_type;
	using pointer_type                           = typename shared_state<T>::pointer_type;
	using reference_type                         = typename shared_state<T>::reference_type;

	bool valid() const                           { return bool(st);                                }
	bool finished() const                        { return !valid() || st->finished;                }
	bool operator!() const                       { return !valid();                                }
	operator bool() const                        { return valid();                                 }

	const shared_state<T> &get_state() const     { return *st;                                     }
	shared_state<T> &get_state()                 { return *st;                                     }

	void set_exception(std::exception_ptr eptr);
	void set_value(const T &val);
	void set_value(T&& val);

	promise();
	promise(promise &&o) noexcept = default;
	promise(const promise &);
	promise &operator=(const promise &) = delete;
	promise &operator=(promise &&) noexcept = default;
	~promise() noexcept;
};

template<>
class ircd::ctx::promise<void>
{
	std::shared_ptr<shared_state<void>> st;

  public:
	using value_type                             = typename shared_state<void>::value_type;

	bool valid() const                           { return bool(st);                                }
	bool finished() const                        { return !valid() || st->finished;                }
	bool operator!() const                       { return !valid();                                }
	operator bool() const                        { return valid();                                 }

	const shared_state<void> &get_state() const  { return *st;                                     }
	shared_state<void> &get_state()              { return *st;                                     }

	void set_exception(std::exception_ptr eptr);
	void set_value();

	promise();
	promise(promise &&o) noexcept = default;
	promise(const promise &);
	promise &operator=(const promise &) = delete;
	promise &operator=(promise &&) noexcept = default;
	~promise() noexcept;
};

inline
ircd::ctx::promise<void>::promise()
:st{std::make_shared<shared_state<void>>()}
{
	++st->promise_refcnt;
	assert(st->promise_refcnt == 1);
}

template<class T>
ircd::ctx::promise<T>::promise()
:st{std::make_shared<shared_state<T>>()}
{
	++st->promise_refcnt;
	assert(st->promise_refcnt == 1);
}

inline
ircd::ctx::promise<void>::promise(const promise<void> &o)
:st{o.st}
{
	if(valid())
	{
		++st->promise_refcnt;
		assert(st->promise_refcnt > 1);
	}
}

template<class T>
ircd::ctx::promise<T>::promise(const promise<T> &o)
:st{o.st}
{
	if(valid())
	{
		++st->promise_refcnt;
		assert(st->promise_refcnt > 1);
	}
}

inline
ircd::ctx::promise<void>::~promise()
noexcept
{
	if(valid())
		if(!--st->promise_refcnt)
			if(!st->finished && !st.unique())
				set_exception(std::make_exception_ptr(broken_promise()));
}

template<class T>
ircd::ctx::promise<T>::~promise()
noexcept
{
	if(valid())
		if(!--st->promise_refcnt)
			if(!st->finished && !st.unique())
				set_exception(std::make_exception_ptr(broken_promise()));
}

template<class T>
void
ircd::ctx::promise<T>::set_value(T&& val)
{
	assert(valid());
	assert(!finished());
	st->val = std::move(val);
	st->finished = true;
	st->cond.notify_all();
	if(st->callback) ircd::post([st(st)]
	{
		st->callback(st->eptr, st->val);
	});
}

inline void
ircd::ctx::promise<void>::set_value()
{
	assert(valid());
	assert(!finished());
	st->finished = true;
	st->cond.notify_all();
	if(st->callback) ircd::post([st(st)]
	{
		st->callback(st->eptr);
	});
}

template<class T>
void
ircd::ctx::promise<T>::set_value(const T &val)
{
	assert(valid());
	assert(!finished());
	st->val = val;
	st->finished = true;
	st->cond.notify_all();
	if(st->callback) ircd::post([st(st)]
	{
		st->callback(st->eptr, st->val);
	});
}

inline void
ircd::ctx::promise<void>::set_exception(std::exception_ptr eptr)
{
	assert(valid());
	assert(!finished());
	st->eptr = std::move(eptr);
	st->finished = true;
	st->cond.notify_all();
	if(st->callback) ircd::post([st(st)]
	{
		st->callback(st->eptr);
	});
}

template<class T>
void
ircd::ctx::promise<T>::set_exception(std::exception_ptr eptr)
{
	assert(valid());
	assert(!finished());
	st->eptr = std::move(eptr);
	st->finished = true;
	st->cond.notify_all();
	if(st->callback) ircd::post([st(st)]
	{
		st->callback(st->eptr, st->val);
	});
}
