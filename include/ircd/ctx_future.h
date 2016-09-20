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
#define HAVE_IRCD_CTX_FUTURE_H

namespace ircd {
namespace ctx  {

enum class future_status
{
	ready,
	timeout,
	deferred,
};

template<class T = void>
class future
{
	std::shared_ptr<shared_state<T>> st;

  public:
	using value_type                             = typename shared_state<T>::value_type;
	using pointer_type                           = typename shared_state<T>::pointer_type;
	using reference_type                         = typename shared_state<T>::reference_type;

	bool valid() const                           { return bool(st);                                }
	bool operator!() const                       { return !valid();                                }
	operator bool() const                        { return valid();                                 }

	template<class time_point> future_status wait_until(const time_point &) const;
	template<class duration> future_status wait(const duration &d) const;
	void wait() const;

	T get();
	operator T()                                 { return get();                                   }

	future();
	future(promise<T> &promise);
};

template<>
class future<void>
{
	std::shared_ptr<shared_state<void>> st;

  public:
	using value_type                             = typename shared_state<void>::value_type;

	bool valid() const                           { return bool(st);                                }
	bool operator!() const                       { return !valid();                                }
	operator bool() const                        { return valid();                                 }

	template<class time_point> future_status wait_until(const time_point &) const;
	template<class duration> future_status wait(const duration &d) const;
	void wait() const;

	future();
	future(promise<void> &promise);
};

template<class... T>
struct scoped_future : future<T...>
{
	template<class... Args> scoped_future(Args&&... args);
	~scoped_future() noexcept;
};

template<class... T>
template<class... Args>
scoped_future<T...>::scoped_future(Args&&... args):
future<T...>{std::forward<Args>(args)...}
{
}

template<class... T>
scoped_future<T...>::~scoped_future()
noexcept
{
	if(std::uncaught_exception())
		return;

	if(this->valid())
		this->wait();
}

inline
future<void>::future():
st(nullptr)
{
}

template<class T>
future<T>::future():
st(nullptr)
{
}

inline
future<void>::future(promise<void> &promise):
st(promise.get_state().share())
{
}

template<class T>
future<T>::future(promise<T> &promise):
st(promise.get_state().share())
{
}

template<class T>
T
future<T>::get()
{
	wait();

	if(unlikely(bool(st->eptr)))
		std::rethrow_exception(st->eptr);

	return st->val;
}

inline void
future<void>::wait()
const
{
	this->wait_until(steady_clock::time_point::max());
}

template<class T>
void
future<T>::wait()
const
{
	this->wait_until(steady_clock::time_point::max());
}

template<class duration>
future_status
future<void>::wait(const duration &d)
const
{
	return this->wait_until(steady_clock::now() + d);
}

template<class T>
template<class duration>
future_status
future<T>::wait(const duration &d)
const
{
	return this->wait_until(steady_clock::now() + d);
}

template<class time_point>
future_status
future<void>::wait_until(const time_point &tp)
const
{
	const auto wfun([this]() -> bool
	{
		return st->finished;
	});

	if(unlikely(!valid()))
		throw no_state();

	if(unlikely(!st->cond.wait_until(tp, wfun)))
		return future_status::timeout;

	return likely(wfun())? future_status::ready:
	                       future_status::deferred;
}

template<class T>
template<class time_point>
future_status
future<T>::wait_until(const time_point &tp)
const
{
	const auto wfun([this]
	{
		return st->finished;
	});

	if(unlikely(!valid()))
		throw no_state();

	if(unlikely(!st->cond.wait_until(tp, wfun)))
		return future_status::timeout;

	return likely(wfun())? future_status::ready:
	                       future_status::deferred;
}

} // namespace ctx
} // namespace ircd
