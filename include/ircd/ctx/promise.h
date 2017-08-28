/*
 * Copyright (C) 2016 Charybdis Development Team
 * Copyright (C) 2016 Jason Volk <jason@zemos.net>
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice is present in all copies.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

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
	bool operator!() const                       { return !valid();                                }
	operator bool() const                        { return valid();                                 }

	const shared_state<T> &get_state() const     { return *st;                                     }
	shared_state<T> &get_state()                 { return *st;                                     }

	void set_exception(std::exception_ptr eptr);
	void set_value(const T &val);
	void set_value(T&& val);
	void reset();

	promise();
	promise(promise &&o) noexcept;
	promise(const promise &) = delete;
	promise &operator=(promise &&o) noexcept;
	promise &operator=(const promise &) = delete;
	~promise() noexcept;
};

template<>
class ircd::ctx::promise<void>
{
	std::shared_ptr<shared_state<void>> st;

  public:
    using value_type                             = typename shared_state<void>::value_type;

	bool valid() const                           { return bool(st);                                }
	bool operator!() const                       { return !valid();                                }
	operator bool() const                        { return valid();                                 }

	const shared_state<void> &get_state() const  { return *st;                                     }
	shared_state<void> &get_state()              { return *st;                                     }

	void set_exception(std::exception_ptr eptr);
	void set_value();
	void reset();

	promise();
	promise(const promise &) = delete;
	promise &operator=(const promise &) = delete;
	~promise() noexcept;
};

inline
ircd::ctx::promise<void>::promise():
st(std::make_shared<shared_state<void>>())
{
}

template<class T>
ircd::ctx::promise<T>::promise():
st(std::make_shared<shared_state<T>>())
{
}

template<class T>
ircd::ctx::promise<T>::promise(promise<T> &&o)
noexcept:
st(std::move(o.st))
{
}

template<class T>
ircd::ctx::promise<T> &
ircd::ctx::promise<T>::operator=(promise<T> &&o)
noexcept
{
	st = std::move(o.st);
	return *this;
}

inline
ircd::ctx::promise<void>::~promise()
noexcept
{
	if(valid() && !st->finished && !st.unique())
		set_exception(std::make_exception_ptr(broken_promise()));
}

template<class T>
ircd::ctx::promise<T>::~promise()
noexcept
{
	if(valid() && !st->finished && !st.unique())
		set_exception(std::make_exception_ptr(broken_promise()));
}

inline void
ircd::ctx::promise<void>::reset()
{
	if(valid())
		st->reset();
}

template<class T>
void
ircd::ctx::promise<T>::reset()
{
	if(valid())
		st->reset();
}

template<class T>
void
ircd::ctx::promise<T>::set_value(T&& val)
{
	st->val = std::move(val);
	st->finished = true;
	st->cond.notify_all();
}

inline void
ircd::ctx::promise<void>::set_value()
{
	st->finished = true;
	st->cond.notify_all();
}

template<class T>
void
ircd::ctx::promise<T>::set_value(const T &val)
{
	st->val = val;
	st->finished = true;
	st->cond.notify_all();
}

inline void
ircd::ctx::promise<void>::set_exception(std::exception_ptr eptr)
{
	st->eptr = std::move(eptr);
	st->finished = true;
	st->cond.notify_all();
}

template<class T>
void
ircd::ctx::promise<T>::set_exception(std::exception_ptr eptr)
{
	st->eptr = std::move(eptr);
	st->finished = true;
	st->cond.notify_all();
}
