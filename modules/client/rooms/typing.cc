// Matrix Construct
//
// Copyright (C) Matrix Construct Developers, Authors & Contributors
// Copyright (C) 2016-2018 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

#include "rooms.h"

using namespace ircd;

struct typist
{
	using is_transparent = void;

	steady_point timesout;
	m::user::id::buf user_id;
	m::room::id::buf room_id;

	bool operator()(const typist &a, const string_view &b) const;
	bool operator()(const string_view &a, const typist &b) const;
	bool operator()(const typist &a, const typist &b) const;
};

ctx::dock timeout_dock;
std::set<typist, typist> typists;
static void timeout_timeout(const typist &);
static void timeout_check();
static void timeout_worker();
context timeout_context
{
	"typing", 128_KiB, context::POST, timeout_worker
};

conf::item<milliseconds>
timeout_max
{
	{ "name",     "ircd.typing.timeout.max" },
	{ "default",  120 * 1000L               },
};

conf::item<milliseconds>
timeout_min
{
	{ "name",     "ircd.typing.timeout.min" },
	{ "default",  15 * 1000L                },
};

conf::item<milliseconds>
timeout_default
{
	{ "name",     "ircd.typing.timeout.default" },
	{ "default",  30 * 1000L                    },
};

static ircd::steady_point calc_timesout(const resource::request &request);

resource::response
put__typing(client &client,
            const resource::request &request,
            const m::room::id &room_id)
{
	if(request.parv.size() < 3)
		throw m::NEED_MORE_PARAMS
		{
			"user_id parameter missing"
		};

	m::user::id::buf user_id
	{
		url::decode(request.parv[2], user_id)
	};

	if(request.user_id != user_id)
		throw m::UNSUPPORTED
		{
			"Typing as someone else not yet supported"
		};

	const bool typing
	{
		request.get("typing", false)
	};

	const m::edu::m_typing event
	{
		{ "room_id",  room_id   },
		{ "typing",   typing    },
		{ "user_id",  user_id   },
	};

	auto it
	{
		typists.lower_bound(user_id)
	};

	const bool was_typing
	{
		it != end(typists) && it->user_id == user_id
	};

	if(typing && !was_typing)
	{
		typists.emplace_hint(it, typist
		{
			calc_timesout(request), user_id, room_id
		});

		timeout_dock.notify_one();
	}
	else if(typing && was_typing)
	{
		auto &t(const_cast<typist &>(*it));
		t.timesout = calc_timesout(request);
	}
	else if(!typing && was_typing)
	{
		typists.erase(it);
	}

	const bool transmit
	{
		(typing && !was_typing) || (!typing && was_typing)
	};

	log::debug
	{
		"Typing %s in %s now[%b] was[%b] xmit[%b]",
		at<"user_id"_>(event),
		at<"room_id"_>(event),
		json::get<"typing"_>(event),
		was_typing,
		transmit
	};

	if(transmit)
		m::typing::set(event);

	return resource::response
	{
		client, http::OK
	};
}

ircd::steady_point
calc_timesout(const resource::request &request)
{
	auto timeout
	{
		request.get("timeout", milliseconds(timeout_default))
	};

	timeout = std::max(timeout, milliseconds(timeout_min));
	timeout = std::min(timeout, milliseconds(timeout_max));
	return now<steady_point>() + timeout;
}

void
timeout_worker()
try
{
	while(1)
	{
		timeout_dock.wait([]
		{
			return !typists.empty();
		});

		timeout_check();
		ctx::sleep(seconds(5));
	}
}
catch(const ctx::interrupted &)
{
	log::debug
	{
		"Typing timeout worker interrupted"
	};
}

void
timeout_check()
{
	const auto now
	{
		ircd::now<steady_point>()
	};

	auto it(begin(typists));
	while(it != end(typists))
		if(it->timesout < now)
		{
			timeout_timeout(*it);
			it = typists.erase(it);
		}
		else ++it;
}

void
timeout_timeout(const typist &t)
{
	const m::edu::m_typing event
	{
		{ "user_id",  t.user_id   },
		{ "room_id",  t.room_id   },
		{ "typing",   false       },
	};

	log::debug
	{
		"Typing timeout for %s in %s",
		string_view{t.user_id},
		string_view{t.room_id}
	};

	m::typing::set(event);
}

bool
typist::operator()(const typist &a, const string_view &b)
const
{
	return a.user_id < b;
}

bool
typist::operator()(const string_view &a, const typist &b)
const
{
	return a < b.user_id;
}

bool
typist::operator()(const typist &a, const typist &b)
const
{
	return a.user_id < b.user_id;
}
