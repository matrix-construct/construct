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

decltype(ircd::m::sync::linear_delta_max)
ircd::m::sync::linear_delta_max
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
		args.since, std::min(args.next_batch, m::vm::current_sequence + 1)
	};

	// When the range indexes are the same, the client is polling for the next
	// event which doesn't exist yet. There is no reason for the since parameter
	// to be greater than that.
	if(range.first > range.second)
		throw m::NOT_FOUND
		{
			"Since parameter is too far in the future..."
		};

	// Keep state for statistics of this sync here on the stack.
	stats stats;
	data data
	{
		request.user_id,
		range,
		&client,
		nullptr,
		&stats,
		args.filter_id
	};

	// Start the chunked encoded response.
	resource::response::chunked response
	{
		client, http::OK, buffer_size
	};

	json::stack out
	{
		response.buf,
		std::bind(&sync::flush, std::ref(data), std::ref(response), ph::_1),
		size_t(flush_hiwat)
	};
	data.out = &out;

	log::debug
	{
		log, "request %s", loghead(data)
	};

	const bool shortpolled
	{
		range.first > range.second?
			false:
		range.second - range.first <= size_t(linear_delta_max)?
			linear_handle(data):
			polylog_handle(data)
	};

	// When shortpoll was successful, do nothing else.
	if(shortpolled)
		return response;

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
	json::stack::object top
	{
		*data.out
	};

	// Empty objects added to output otherwise Riot b0rks.
	json::stack::object
	{
		*data.out, "rooms"
	};

	json::stack::object
	{
		*data.out, "presence"
	};

	json::stack::member
	{
		*data.out, "next_batch", json::value
		{
			lex_cast(data.range.second), json::STRING
		}
	};
}

ircd::const_buffer
ircd::m::sync::flush(data &data,
                     resource::response::chunked &response,
                     const const_buffer &buffer)
{
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

// polylog
//
// Random access approach for large `since` ranges. The /sync schema itself is
// recursed. For every component in the schema, the handler seeks the events
// appropriate for the user and appends it to the output. Concretely, this
// involves a full iteration of the rooms a user is a member of, and a full
// iteration of the presence status for all users visible to a user, etc.
//
// This entire process occurs in a single pass. The schema is traced with
// json::stack and its buffer is flushed to the client periodically with
// chunked encoding.

bool
ircd::m::sync::polylog_handle(data &data)
try
{
	json::stack::checkpoint checkpoint
	{
		*data.out
	};

	json::stack::object top
	{
		*data.out
	};

	bool ret{false};
	m::sync::for_each(string_view{}, [&data, &ret]
	(item &item)
	{
		json::stack::checkpoint checkpoint
		{
			*data.out
		};

		json::stack::object object
		{
			*data.out, item.member_name()
		};

		if(item.polylog(data))
			ret = true;
		else
			checkpoint.rollback();

		return true;
	});

	if(ret)
		json::stack::member
		{
			*data.out, "next_batch", json::value
			{
				lex_cast(data.range.second), json::STRING
			}
		};

	if(!ret)
		checkpoint.rollback();

	if(stats_info) log::info
	{
		log, "polylog %s commit:%b complete",
		loghead(data),
		ret
	};

	return ret;
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
// Approach for small `since` ranges. The range of events is iterated and
// the event itself is presented to each handler in the schema. This also
// involves a json::stack trace of the schema so that if the handler determines
// the event is appropriate for syncing to the user the output buffer will
// contain a residue of a /sync response with a single event.
//
// After the iteration of events is complete we are left with several buffers
// of properly formatted individual /sync responses which we rewrite into a
// single response to overcome the inefficiency of request ping-pong under
// heavy load.

namespace ircd::m::sync
{
	static bool linear_proffer_event_one(data &);
	static size_t linear_proffer_event(data &, const mutable_buffer &);
	static event::idx linear_proffer(data &, window_buffer &);
}

bool
ircd::m::sync::linear_handle(data &data)
try
{
	json::stack::checkpoint checkpoint
	{
		*data.out
	};

	json::stack::object top
	{
		*data.out
	};

	const unique_buffer<mutable_buffer> buf
	{
		96_KiB //TODO: XXX
	};

	window_buffer wb{buf};
	const event::idx last
	{
		linear_proffer(data, wb)
	};

	const json::vector vector
	{
		wb.completed()
	};

	if(last)
	{
		json::stack::member
		{
			top, "next_batch", json::value
			{
				lex_cast(last+1), json::STRING
			}
		};

		json::merge(top, vector);
	}
	else checkpoint.rollback();

	log::debug
	{
		log, "linear %s last:%lu complete",
		loghead(data),
		last
	};

	return last;
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

/// Iterates the events in the data.range and creates a json::vector in
/// the supplied window_buffer. The return value is the event_idx of the
/// last event which fit in the buffer, or 0 of nothing was of interest
/// to our client in the event iteration.
ircd::m::event::idx
ircd::m::sync::linear_proffer(data &data,
                              window_buffer &wb)
{
	event::idx ret(0);
	m::events::for_each(data.range, [&data, &wb, &ret]
	(const m::event::idx &event_idx, const m::event &event)
	{
		const scope_restore their_event
		{
			data.event, &event
		};

		const scope_restore their_event_idx
		{
			data.event_idx, event_idx
		};

		wb([&data, &ret, &event_idx]
		(const mutable_buffer &buf)
		{
			const auto consumed
			{
				linear_proffer_event(data, buf)
			};

			if(consumed)
				ret = event_idx;

			return consumed;
		});

		return wb.remaining() >= 65_KiB; //TODO: XXX
	});

	return ret;
}

/// Sets up a json::stack for the iteration of handlers for
/// one event.
size_t
ircd::m::sync::linear_proffer_event(data &data,
                                    const mutable_buffer &buf)
{
	json::stack out{buf};
	const scope_restore their_out
	{
		data.out, &out
	};

	json::stack::object top
	{
		*data.out
	};

	const bool success
	{
		linear_proffer_event_one(data)
	};

	top.~object();
	return success?
		size(out.completed()):
		0UL;
}

/// Generates a candidate /sync response for a single event by
/// iterating all of the handlers.
bool
ircd::m::sync::linear_proffer_event_one(data &data)
{
	return !m::sync::for_each(string_view{}, [&data]
	(item &item)
	{
		json::stack::checkpoint checkpoint
		{
			*data.out
		};

		json::stack::object object
		{
			*data.out, item.member_name()
		};

		if(item.linear(data))
			return false;

		checkpoint.rollback();
		return true;
	});
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
	const scope_restore their_event
	{
		data.event, &event
	};

	const scope_restore their_event_idx
	{
		data.event_idx, event.event_idx
	};

	json::stack::checkpoint checkpoint
	{
		*data.out
	};

	json::stack::object top
	{
		*data.out
	};

	const bool ret
	{
		linear_proffer_event_one(data)
	};

	if(ret)
	{
		json::stack::member
		{
			*data.out, "next_batch", json::value
			{
				lex_cast(event.event_idx + 1), json::STRING
			}
		};

		log::debug
		{
			log, "longpoll %s idx:%lu complete",
			loghead(data),
			event.event_idx
		};
	}
	else checkpoint.rollback();

	return ret;
}

//
// sync/args.h
//

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
// args::args
//

ircd::m::sync::args::args(const resource::request &request)
try
:request
{
    request
}
{
}
catch(const bad_lex_cast &e)
{
    throw m::BAD_REQUEST
    {
        "Since parameter invalid :%s", e.what()
    };
}
