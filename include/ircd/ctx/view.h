/*
 *  Copyright (C) 2016 Charybdis Development Team
 *  Copyright (C) 2016 Jason Volk <jason@zemos.net>
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
#define HAVE_IRCD_CTX_VIEW_H

namespace ircd::ctx
{
	template<class T> class view;
}

/// Device for a context to share data on its stack with others while yielding
///
/// The view yields a context while other contexts examine the object pointed
/// to in the view. This allows a producing context to construct something
/// on its stack and then wait for the consuming contexts to do something with
/// that data before the producer resumes and potentially destroys the data.
/// This creates a very simple and lightweight single-producer/multi-consumer
/// queue mechanism using only context switching.
///
/// Consumers get one chance to safely view the data when a call to wait()
/// returns. Once the consumer context yields again for any reason the data is
/// potentially invalid. The data can only be viewed once by the consumer
/// because the second call to wait() will yield until the next data is
/// made available by the producer, not the same data.
///
/// Producers will share an object during the call to notify(). Once the call
/// to notify() returns all consumers have viewed the data and the producer is
/// free to destroy it.
///
template<class T>
class ircd::ctx::view
{
	T *t {nullptr};
	dock a, b;

	bool ready() const;

  public:
	size_t waiting() const;

	// Consumer interface;
	template<class time_point> T &wait_until(time_point&&);
	template<class duration> T &wait_for(const duration &);
	T &wait();

	// Producer interface;
	void notify(T &);

	view() = default;
	~view() noexcept;
};

template<class T>
ircd::ctx::view<T>::~view()
noexcept
{
	assert(!waiting());
}

template<class T>
void
ircd::ctx::view<T>::notify(T &t)
{
	const scope afterward{[this]
	{
		assert(a.empty());
		this->t = nullptr;
		if(!b.empty())
		{
			b.notify_all();
			yield();
		}
	}};

	assert(b.empty());
	this->t = &t;
	a.notify_all();
	yield();
}

template<class T>
T &
ircd::ctx::view<T>::wait()
{
	b.wait([this]
	{
		return !ready();
	});

	a.wait([this]
	{
		return ready();
	});

	assert(t != nullptr);
	return *t;
}

template<class T>
template<class duration>
T &
ircd::ctx::view<T>::wait_for(const duration &dur)
{
	return wait_until(now<steady_point>() + dur);
}

template<class T>
template<class time_point>
T &
ircd::ctx::view<T>::wait_until(time_point&& tp)
{
	if(!b.wait_until(tp, [this]
	{
		return !ready();
	}))
		throw timeout();

	if(!a.wait_until(tp, [this]
	{
		return ready();
	}))
		throw timeout();

	assert(t != nullptr);
	return *t;
}

template<class T>
size_t
ircd::ctx::view<T>::waiting()
const
{
	return a.size() + b.size();
}

template<class T>
bool
ircd::ctx::view<T>::ready()
const
{
	return t != nullptr;
}
