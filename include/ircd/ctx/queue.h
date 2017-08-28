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
#define HAVE_IRCD_CTX_QUEUE_H

namespace ircd::ctx
{
	template<class T> class queue;
}

template<class T>
class ircd::ctx::queue
{
	struct dock dock;
	std::queue<T> q;

  public:
	auto empty() const                           { return q.empty();                               }
	auto size() const                            { return q.size();                                }

	// Consumer interface; waits for item and std::move() it off the queue
	template<class time_point> T pop_until(time_point&&);
	template<class duration> T pop_for(const duration &);
	T pop();

	// Producer interface; emplace item on the queue and notify consumer
	template<class... args> void emplace(args&&...);
	void push(const T &);
	void push(T &&) noexcept;

	queue() = default;
	~queue() noexcept;
};

template<class T>
ircd::ctx::queue<T>::~queue()
noexcept
{
	assert(q.empty());
}

template<class T>
void
ircd::ctx::queue<T>::push(T &&t)
noexcept
{
	q.push(std::move(t));
	dock.notify();
}

template<class T>
void
ircd::ctx::queue<T>::push(const T &t)
{
	q.push(t);
	dock.notify();
}

template<class T>
template<class... args>
void
ircd::ctx::queue<T>::emplace(args&&... a)
{
	q.emplace(std::forward<args>(a)...);
	dock.notify();
}

template<class T>
T
ircd::ctx::queue<T>::pop()
{
	dock.wait([this]
	{
		return !q.empty();
	});

	const scope pop([this]
	{
		q.pop();
	});

	auto &ret(q.front());
	return std::move(ret);
}

template<class T>
template<class duration>
T
ircd::ctx::queue<T>::pop_for(const duration &dur)
{
	const auto status(dock.wait_for(dur, [this]
	{
		return !q.empty();
	}));

	if(status == cv_status::timeout)
		throw timeout();

	const scope pop([this]
	{
		q.pop();
	});

	auto &ret(q.front());
	return std::move(ret);
}

template<class T>
template<class time_point>
T
ircd::ctx::queue<T>::pop_until(time_point&& tp)
{
	const auto status(dock.wait_until(tp, [this]
	{
		return !q.empty();
	}));

	if(status == cv_status::timeout)
		throw timeout();

	const scope pop([this]
	{
		q.pop();
	});

	auto &ret(q.front());
	return std::move(ret);
}
