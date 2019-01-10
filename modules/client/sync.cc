// Matrix Construct
//
// Copyright (C) Matrix Construct Developers, Authors & Contributors
// Copyright (C) 2016-2018 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

#include "sync.h"

ircd::mapi::header
IRCD_MODULE
{
	"Client 6.2.1 :Sync"
};

decltype(ircd::m::sync::resource)
ircd::m::sync::resource
{
	"/_matrix/client/r0/sync",
	{
		description
	}
};

decltype(ircd::m::sync::description)
ircd::m::sync::description
{R"(6.2.1

Synchronise the client's state with the latest state on the server. Clients
use this API when they first log in to get an initial snapshot of the state
on the server, and then continue to call this API to get incremental deltas
to the state, and to receive new messages.
)"};

ircd::conf::item<ircd::milliseconds>
ircd::m::sync::args::timeout_max
{
	{ "name",     "ircd.client.sync.timeout.max"  },
	{ "default",  15 * 1000L                      },
};

ircd::conf::item<ircd::milliseconds>
ircd::m::sync::args::timeout_min
{
	{ "name",     "ircd.client.sync.timeout.min"  },
	{ "default",  5 * 1000L                       },
};

ircd::conf::item<ircd::milliseconds>
ircd::m::sync::args::timeout_default
{
	{ "name",     "ircd.client.sync.timeout.default"  },
	{ "default",  10 * 1000L                          },
};

//
// GET sync
//

decltype(ircd::m::sync::method_get)
ircd::m::sync::method_get
{
	resource, "GET", handle_get,
	{
		method_get.REQUIRES_AUTH,
		-1s,
	}
};

ircd::resource::response
ircd::m::sync::handle_get(client &client,
                          const resource::request &request)
try
{
	const args args
	{
		request
	};

	const std::pair<event::idx, event::idx> range
	{
		args.since,                  // start at since token
		m::vm::current_sequence      // stop at present
	};

	stats stats;
	data data
	{
		stats, client, request.user_id, range, args.filter_id
	};

	log::debug
	{
		log, "request %s", loghead(data)
	};

	if(data.since > data.current + 1)
		throw m::NOT_FOUND
		{
			"Since parameter is too far in the future..."
		};

	const size_t linear_delta_max
	{
		linear::delta_max
	};

	const bool shortpolled
	{
		range.first > range.second?
			false:
		range.second - range.first <= linear_delta_max?
			polylog::handle(data):
			polylog::handle(data)
	};

	// When shortpoll was successful, do nothing else.
	if(shortpolled)
		return {};

	// When longpoll was successful, do nothing else.
	if(longpoll::poll(data, args))
		return {};

	// A user-timeout occurred. According to the spec we return a
	// 200 with empty fields rather than a 408.
	const json::value next_batch
	{
		lex_cast(m::vm::current_sequence + 0), json::STRING
	};

	return resource::response
	{
		client, json::members
		{
			{ "next_batch",  next_batch      },
			{ "rooms",       json::object{}  },
			{ "presence",    json::object{}  },
		}
	};
}
catch(const bad_lex_cast &e)
{
	throw m::BAD_REQUEST
	{
		"Since parameter invalid :%s", e.what()
	};
}

//
// polylog
//

bool
ircd::m::sync::polylog::handle(data &data)
try
{
	json::stack::object object
	{
		data.out
	};

	m::sync::for_each(string_view{}, [&data]
	(item &item)
	{
		json::stack::member member
		{
			data.out, item.member_name()
		};

		item.polylog(data);
		return true;
	});

	const json::value next_batch
	{
		lex_cast(data.current + 1), json::STRING
	};

	json::stack::member
	{
		object, "next_batch", next_batch
	};

	log::info
	{
		log, "polylog %s (next_batch:%s)",
		loghead(data),
		string_view{next_batch}
	};

	return data.committed();
}
catch(const std::exception &e)
{
	log::error
	{
		log, "polylog %s FAILED :%s",
		loghead(data),
		e.what()
	};

	throw;
}

//
// linear
//

decltype(ircd::m::sync::linear::delta_max)
ircd::m::sync::linear::delta_max
{
	{ "name",     "ircd.client.sync.linear.delta.max" },
	{ "default",  1024                                },
};

bool
ircd::m::sync::linear::handle(data &data)
try
{
	json::stack::object object
	{
		data.out
	};

	const m::events::range range
	{
		data.since, data.current + 1
	};

	m::events::for_each(range, [&data]
	(const m::event::idx &event_idx, const m::event &event)
	{
		const scope_restore<decltype(data.event)> theirs
		{
			data.event, &event
		};

		m::sync::for_each(string_view{}, [&data]
		(item &item)
		{
			json::stack::member member
			{
				data.out, item.member_name()
			};

			item.linear(data);
			return true;
		});

		return true;
	});

	const json::value next_batch
	{
		lex_cast(data.current + 1), json::STRING
	};

	json::stack::member
	{
		object, "next_batch", next_batch
	};

	log::debug
	{
		log, "linear %s (next_batch:%s)",
		loghead(data),
		string_view{next_batch}
	};

	return data.committed();
}
catch(const std::exception &e)
{
	log::error
	{
		log, "linear %s FAILED :%s",
		loghead(data),
		e.what()
	};

	throw;
}

//
// longpoll
//

decltype(ircd::m::sync::longpoll::notified)
ircd::m::sync::longpoll::notified
{
	handle_notify,
	{
		{ "_site",  "vm.notify" },
	}
};

void
ircd::m::sync::longpoll::handle_notify(const m::event &event,
                                       m::vm::eval &eval)
{
	assert(eval.opts);
	if(!eval.opts->notify_clients)
		return;

	if(!polling)
	{
		queue.clear();
		return;
	}

	queue.emplace_back(eval);
	dock.notify_all();
}

bool
ircd::m::sync::longpoll::poll(data &data,
                              const args &args)
{
	++polling;
	const unwind unpoll{[]
	{
		--polling;
	}};

	while(1)
	{
		if(!dock.wait_until(args.timesout))
			return false;

		if(queue.empty())
			continue;

		const auto &accepted
		{
			queue.front()
		};

		const unwind pop{[]
		{
			if(polling <= 1)
				queue.pop_front();
		}};

		if(handle(data, args, accepted))
			return true;
	}
}

bool
ircd::m::sync::longpoll::handle(data &data,
                                const args &args,
                                const accepted &event)
{
	return false;
}
