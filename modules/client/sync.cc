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

const auto linear_delta_max_help
{R"(

Maximum number of events to scan sequentially for a /sync. This determines
whether linear-sync or polylog-sync mode is used to satisfy the request. If
the difference between the since token (lower-bound) and the upper-bound of
the sync is within this value, the linear-sync mode is used. If it is more
than this value a polylog-sync mode is used. The latter is used because at
some threshold it becomes too expensive to scan a huge number of events to
grab only those that the client requires; it is cheaper to conduct a series
of random-access queries with polylog-sync instead. Note the exclusive
upper-bound of a sync is determined either by a non-spec query parameter
'next_batch' or the vm::sequence::retired+1.

)"};

const auto linear_buffer_size_help
{R"(

The size of the coalescing buffer used when conducting a linear-sync. During
the sequential scan of events, when an event is marked as required for the
client's sync it is stringified and appended to this buffer. The buffer has
the format of a json::vector of individual events. At the end of the linear
sync, the objects in this buffer are merged into a single spec /sync response.

When this buffer is full the linear sync must finish and respond to the client
with whatever it has. The event::idx of the last event that fit into the buffer
forms the basis for the next_batch so the client can continue with another linear
/sync to complete the range.

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
	{ "help",     "Response chunk buffer size"   },
};

decltype(ircd::m::sync::linear_buffer_size)
ircd::m::sync::linear_buffer_size
{
	{ "name",     "ircd.client.sync.linear.buffer_size" },
	{ "default",  long(96_KiB)                          },
	{ "help",     linear_buffer_size_help               },
};

decltype(ircd::m::sync::linear_delta_max)
ircd::m::sync::linear_delta_max
{
	{ "name",     "ircd.client.sync.linear.delta.max"  },
	{ "default",  1024                                 },
	{ "help",     linear_delta_max_help                },
};

decltype(ircd::m::sync::polylog_only)
ircd::m::sync::polylog_only
{
	{ "name",     "ircd.client.sync.polylog_only" },
	{ "default",  false                           },
};

decltype(ircd::m::sync::longpoll_enable)
ircd::m::sync::longpoll_enable
{
	{ "name",     "ircd.client.sync.longpoll.enable" },
	{ "default",  true                               },
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
	// exist yet because it is one past the server's sequence::retired counter.
	const m::events::range range
	{
		args.since, std::min(args.next_batch, m::vm::sequence::retired + 1)
	};

	// When the range indexes are the same, the client is polling for the next
	// event which doesn't exist yet. There is no reason for the since parameter
	// to be greater than that.
	if(range.first > range.second)
		throw m::NOT_FOUND
		{
			"Since parameter '%lu' is too far in the future."
			" Cannot be greater than '%lu'.",
			range.first,
			range.second
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

	const bool should_longpoll
	{
		range.first > vm::sequence::retired
	};

	const bool should_linear
	{
		!should_longpoll &&
		!bool(polylog_only) &&
		range.second - range.first <= size_t(linear_delta_max)
	};

	const bool shortpolled
	{
		should_longpoll?
			false:
		should_linear?
			linear_handle(data):
			polylog_handle(data)
	};

	// When shortpoll was successful, do nothing else.
	if(shortpolled)
		return {};

	if(longpoll_enable && longpoll::poll(data, args))
		return {};

	const auto &next_batch
	{
		polylog_only?
			data.range.first:
			data.range.second
	};

	// A user-timeout occurred. According to the spec we return a
	// 200 with empty fields rather than a 408.
	empty_response(data, next_batch);
	return {};
}

void
ircd::m::sync::empty_response(data &data,
                              const uint64_t &next_batch)
{
	json::stack::object top
	{
		*data.out
	};

	// Empty objects added to output otherwise Riot b0rks.
	json::stack::object
	{
		top, "rooms"
	};

	json::stack::object
	{
		top, "presence"
	};

	json::stack::member
	{
		top, "next_batch", json::value
		{
			lex_cast(next_batch), json::STRING
		}
	};

	log::debug
	{
		log, "request %s timeout @%lu",
		loghead(data),
		next_batch
	};
}

ircd::const_buffer
ircd::m::sync::flush(data &data,
                     resource::response::chunked &response,
                     const const_buffer &buffer)
{
	const auto wrote
	{
		response.flush(buffer)
	};

	if(data.stats)
	{
		data.stats->flush_bytes += size(wrote);
		data.stats->flush_count++;
	}

	return wrote;
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
		{
			ret = true;
			data.out->invalidate_checkpoints();
		}
		else checkpoint.decommit();

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
		checkpoint.decommit();

	if(stats_info) log::info
	{
		log, "request %s polylog commit:%b complete @%lu",
		loghead(data),
		ret,
		data.range.second
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
	static std::pair<event::idx, bool> linear_proffer(data &, window_buffer &);
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
		// must be at least worst-case size of m::event plus some.
		std::max(size_t(linear_buffer_size), size_t(96_KiB))
	};

	window_buffer wb{buf};
	const auto &[last, completed]
	{
		linear_proffer(data, wb)
	};

	const json::vector vector
	{
		wb.completed()
	};

	const auto next
	{
		last && completed?
			data.range.second:
		last?
			std::min(last + 1, data.range.second):
			0UL
	};

	if(last)
	{
		json::stack::member
		{
			top, "next_batch", json::value
			{
				lex_cast(next), json::STRING
			}
		};

		json::merge(top, vector);
	}
	else checkpoint.decommit();

	log::debug
	{
		log, "request %s linear last:%lu %s@%lu",
		loghead(data),
		last,
		completed? "complete "_sv : string_view{},
		next
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
std::pair<ircd::m::event::idx, bool>
ircd::m::sync::linear_proffer(data &data,
                              window_buffer &wb)
{
	event::idx ret(0);
	const auto closure{[&data, &wb, &ret]
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

		const bool enough_space_for_more
		{
			// The buffer must have at least this much more space
			// to continue with the iteration. Otherwise if the next
			// worst-case event does not fit, bad things.
			wb.remaining() >= 68_KiB
		};

		return enough_space_for_more;
	}};

	const auto completed
	{
		m::events::for_each(data.range, closure)
	};

	return
	{
		ret, completed
	};
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
	bool ret{false};
	m::sync::for_each(string_view{}, [&data, &ret]
	(item &item)
	{
		json::stack::checkpoint checkpoint
		{
			*data.out
		};

		if(item.linear(data))
			ret = true;
		else
			checkpoint.rollback();

		return true;
	});

	return ret;
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
try
{
	const unique_buffer<mutable_buffer> scratch
	{
		96_KiB
	};

	const scope_count polling{longpoll::polling}; do
	{
		if(!dock.wait_until(args.timesout))
			break;

		assert(data.client && data.client->sock);
		check(*data.client->sock);

		if(queue.empty())
			continue;

		const auto &accepted
		{
			queue.front()
		};

		const unwind pop{[]
		{
			if(longpoll::polling <= 1)
				queue.pop_front();
		}};

		if(polylog_only)
			return false;

		if(handle(data, args, accepted, scratch))
			return true;
	}
	while(1);

	return false;
}
catch(const std::system_error &e)
{
	log::derror
	{
		log, "longpoll %s failed :%s",
		loghead(data),
		e.what()
	};

	throw;
}
catch(const std::exception &e)
{
	log::error
	{
		log, "longpoll %s FAILED :%s",
		loghead(data),
		e.what()
	};

	throw;
}

bool
ircd::m::sync::longpoll::handle(data &data,
                                const args &args,
                                const accepted &event,
                                const mutable_buffer &scratch)
{
	const scope_restore their_event
	{
		data.event, &event
	};

	const scope_restore their_event_idx
	{
		data.event_idx, event.event_idx
	};

	const scope_restore client_txnid
	{
		data.client_txnid, event.client_txnid
	};

	const size_t consumed
	{
		linear_proffer_event(data, scratch)
	};

	if(!consumed)
		return false;

	const json::vector vector
	{
		string_view
		{
			buffer::data(scratch), consumed
		}
	};

	json::stack::object top
	{
		*data.out
	};

	json::merge(top, vector);

	const auto next
	{
		data.event_idx?
			std::min(data.event_idx + 1, vm::sequence::retired + 1):
			data.range.first
	};

	json::stack::member
	{
		top, "next_batch", json::value
		{
			lex_cast(next), json::STRING
		}
	};

	log::debug
	{
		log, "request %s longpoll hit:%lu complete @%lu",
		loghead(data),
		event.event_idx,
		next
	};

	return true;
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
