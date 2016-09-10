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
#define HAVE_IRCD_CTX_DOCK_H

namespace ircd {
namespace ctx  {

enum class cv_status
{
	no_timeout, timeout
};

class dock
{
	std::deque<ctx *> q;

	void remove_self();

  public:
	auto size() const                            { return q.size();                                }

	template<class time_point, class predicate> bool wait_until(time_point&& tp, predicate&& pred);
	template<class time_point> cv_status wait_until(time_point&& tp);

	template<class duration, class predicate> bool wait_for(const duration &dur, predicate&& pred);
	template<class duration> cv_status wait_for(const duration &dur);

	template<class predicate> void wait(predicate&& pred);
	void wait();

	void notify_all() noexcept;
	void notify_one() noexcept;

	dock() = default;
	~dock() noexcept;
};

inline
dock::~dock()
noexcept
{
	assert(q.empty());
}

inline void
dock::notify_one()
noexcept
{
	if(q.empty())
		return;

	notify(*q.front());
}

inline void
dock::notify_all()
noexcept
{
	const auto copy(q);
	for(const auto &c : copy)
		notify(*c);
}

inline void
dock::wait()
{
	const scope remove(std::bind(&dock::remove_self, this));
	q.emplace_back(&cur());
	ircd::ctx::wait();
}

template<class predicate>
void
dock::wait(predicate&& pred)
{
	if(pred())
		return;

	const scope remove(std::bind(&dock::remove_self, this));
	q.emplace_back(&cur()); do
	{
		ircd::ctx::wait();
	}
	while(!pred());
}

template<class duration>
cv_status
dock::wait_for(const duration &dur)
{
	static const duration zero(0);

	const scope remove(std::bind(&dock::remove_self, this));
	q.emplace_back(&cur());

	return ircd::ctx::wait<std::nothrow_t>(dur) > zero? cv_status::no_timeout:
	                                                    cv_status::timeout;
}

template<class duration,
         class predicate>
bool
dock::wait_for(const duration &dur,
               predicate&& pred)
{
	static const duration zero(0);

	if(pred())
		return true;

	const scope remove(std::bind(&dock::remove_self, this));
	q.emplace_back(&cur()); do
	{
		const bool expired(ircd::ctx::wait<std::nothrow_t>(dur) <= zero);

		if(pred())
			return true;

		if(expired)
			return false;
	}
	while(1);
}

template<class time_point>
cv_status
dock::wait_until(time_point&& tp)
{
	const scope remove(std::bind(&dock::remove_self, this));
	q.emplace_back(&cur());

	return ircd::ctx::wait_until<std::nothrow_t>(tp)? cv_status::timeout:
	                                                  cv_status::no_timeout;
}

template<class time_point,
         class predicate>
bool
dock::wait_until(time_point&& tp,
                 predicate&& pred)
{
	if(pred())
		return true;

	const scope remove(std::bind(&dock::remove_self, this));
	q.emplace_back(&cur()); do
	{
		const bool expired(ircd::ctx::wait_until<std::nothrow_t>(tp));

		if(pred())
			return true;

		if(expired)
			return false;
	}
	while(1);
}

inline void
dock::remove_self()
{
	const auto it(std::find(begin(q), end(q), &cur()));
	assert(it != end(q));
	q.erase(it);
}

} // namespace ctx
} // namespace ircd
