/*
 * charybdis: 21st Century IRC++d
 *
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
 *
 */

#pragma once
#define HAVE_IRCD_M_EVENTS_H

namespace ircd::m::events
{
	IRCD_M_EXCEPTION(m::error, INVALID_TRANSITION, http::BAD_REQUEST)

	struct transition;

	using transition_list = std::list<struct transition *>;
	extern transition_list transitions;

	extern database *events;
	using cursor = db::cursor<events, m::event>;
	using const_iterator = cursor::const_iterator;
	using iterator = const_iterator;
	using where = cursor::where_type;

	const_iterator find(const id &);
	const_iterator begin();
	const_iterator end();

	void insert(const event &);
	void insert(json::iov &);
}

struct ircd::m::events::transition
{
	struct method
	{
		std::function<bool (const event &)> valid;
		std::function<void (const event &) noexcept> effects;
	};

	const char *name;
	struct method method;
	unique_const_iterator<events::transition_list> it;

	virtual bool valid(const event &event) const;
	virtual void effects(const event &e);

	transition(const char *const &name, struct method method);
	transition(const char *const &name);
	virtual ~transition() noexcept;
};
