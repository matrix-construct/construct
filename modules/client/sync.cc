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

decltype(ircd::m::sync::flush_hiwat)
ircd::m::sync::flush_hiwat
{
	{ "name",     "ircd.client.sync.flush.hiwat" },
	{ "default",  long(48_KiB)                   },
};

decltype(ircd::m::sync::buffer_size)
ircd::m::sync::buffer_size
{
	{ "name",     "ircd.client.sync.buffer_size" },
	{ "default",  long(128_KiB)                  },
};

decltype(ircd::m::sync::linear::delta_max)
ircd::m::sync::linear::delta_max
{
	{ "name",     "ircd.client.sync.linear.delta.max" },
	{ "default",  1024                                },
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
{
	// Parse the request options
	const args args
	{
		request
	};

	// The range to `/sync`. We involve events starting at the range.first
	// index in this sync. We will not involve events with an index equal
	// or greater than the range.second. In this case the range.second does not
	// exist yet because it is one past the server's current_sequence counter.
	const m::events::range range
	{
		args.since,                  // start at since token
		m::vm::current_sequence + 1  // stop before future
	};

	// When the range indexes are the same, the client is polling for the next
	// event which doesn't exist yet. There is no reason for the since parameter
	// to be greater than that.
	if(range.first > range.second)
		throw m::NOT_FOUND
		{
			"Since parameter is too far in the future..."
		};

	// Setup an output buffer to compose the response. This has to be at least
	// the worst-case size of a matrix event (64_KiB) or bad things happen.
	const unique_buffer<mutable_buffer> buffer
	{
		size_t(buffer_size)
	};

	// Setup a chunked encoded response.
	resource::response::chunked response
	{
		client, http::OK
	};

	// Keep state for statistics of this sync here on the stack.
	stats stats;
	data data
	{
		request.user_id,
		range,
		buffer,
		std::bind(&sync::flush, std::ref(data), std::ref(response), ph::_1),
		size_t(flush_hiwat),
		&client,
		&stats,
		args.filter_id
	};

	log::debug
	{
		log, "request %s", loghead(data)
	};

	json::stack::object object
	{
		data.out
	};

	const bool shortpolled
	{
		range.first > range.second?
			false:
		range.second - range.first <= size_t(linear::delta_max)?
			polylog::handle(data):
			polylog::handle(data)
	};

	// When shortpoll was successful, do nothing else.
	if(shortpolled)
		return response;

	// When longpoll was successful, do nothing else.
	if(longpoll::poll(data, args))
		return response;

	// A user-timeout occurred. According to the spec we return a
	// 200 with empty fields rather than a 408.
	empty_response(data);
	return response;
}

void
ircd::m::sync::empty_response(data &data)
{
	data.commit();
	json::stack::member
	{
		data.out, "next_batch", json::value
		{
			lex_cast(data.range.second), json::STRING
		}
	};

	// Empty objects added to output otherwise Riot b0rks.
	json::stack::object{data.out, "rooms"};
	json::stack::object{data.out, "presence"};
}

ircd::const_buffer
ircd::m::sync::flush(data &data,
                     resource::response::chunked &response,
                     const const_buffer &buffer)
{
	if(!data.committed)
		return const_buffer
		{
			buffer::data(buffer), 0UL
		};

	const size_t wrote
	{
		response.write(buffer)
	};

	if(data.stats)
	{
		data.stats->flush_bytes += wrote;
		data.stats->flush_count++;
	}

	return const_buffer
	{
		buffer::data(buffer), wrote
	};
}

//
// polylog
//

bool
ircd::m::sync::polylog::handle(data &data)
try
{
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

	json::stack::member
	{
		data.out, "next_batch", json::value
		{
			lex_cast(data.range.second), json::STRING
		}
	};

	log::info
	{
		log, "polylog %s complete", loghead(data)
	};

	return data.committed;
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

bool
ircd::m::sync::linear::handle(data &data)
try
{
	m::events::for_each(data.range, [&data]
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

	json::stack::member
	{
		data.out, "next_batch", json::value
		{
			lex_cast(data.range.second), json::STRING
		}
	};

	log::debug
	{
		log, "linear %s complete", loghead(data)
	};

	return data.committed;
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
