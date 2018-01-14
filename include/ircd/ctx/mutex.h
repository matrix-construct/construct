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
#define HAVE_IRCD_CTX_MUTEX_H

namespace ircd::ctx
{
	class mutex;

	template<class queue> void release_sequence(queue &);
}

//
// The mutex only allows one context to lock it and continue,
// additional contexts are queued. This can be used with std::
// locking concepts.
//
class ircd::ctx::mutex
{
	bool m;
	list q;

  public:
	bool try_lock();
	template<class time_point> bool try_lock_until(const time_point &);
	template<class duration> bool try_lock_for(const duration &);

	void lock();
	void unlock();

	mutex();
	~mutex() noexcept;
};

inline
ircd::ctx::mutex::mutex():
m(false)
{
}

inline
ircd::ctx::mutex::~mutex()
noexcept
{
	assert(!m);
	assert(q.empty());
}

inline void
ircd::ctx::mutex::unlock()
{
	assert(m);
	m = false;
	release_sequence(q);
}

inline void
ircd::ctx::mutex::lock()
{
	if(likely(try_lock()))
		return;

	q.push_back(current);
	while(!try_lock())
		wait();
}

template<class duration>
bool
ircd::ctx::mutex::try_lock_for(const duration &d)
{
	return try_lock_until(steady_clock::now() + d);
}

template<class time_point>
bool
ircd::ctx::mutex::try_lock_until(const time_point &tp)
{
	if(likely(try_lock()))
		return true;

	q.push_back(current);
	while(!try_lock())
	{
		if(unlikely(wait_until<std::nothrow_t>(tp)))
		{
			q.remove(current);
			return false;
		}
	}

	return true;
}

inline bool
ircd::ctx::mutex::try_lock()
{
	if(m)
		return false;

	m = true;
	return true;
}

template<class queue>
void
ircd::ctx::release_sequence(queue &q)
{
	ctx *next; do
	{
		if(!q.empty())
		{
			next = q.front();
			q.pop_front();
		}
		else next = nullptr;
	}
	while(next == current);

	if(next)
		yield(*next);
}
