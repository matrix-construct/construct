// Matrix Construct
//
// Copyright (C) Matrix Construct Developers, Authors & Contributors
// Copyright (C) 2016-2018 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

using namespace ircd;

mapi::header
IRCD_MODULE
{
	"Federation (undocumented) :Get missing events."
};

m::resource
get_missing_events_resource
{
	"/_matrix/federation/v1/get_missing_events/",
	{
		"Federation (undocumented) missing events handler",
		resource::DIRECTORY,
	}
};

static m::resource::response get__missing_events(client &, const m::resource::request &);

m::resource::method
method_get
{
	get_missing_events_resource, "GET", get__missing_events,
	{
		method_get.VERIFY_ORIGIN
	}
};

m::resource::method
method_post
{
	get_missing_events_resource, "POST", get__missing_events,
	{
		method_post.VERIFY_ORIGIN
	}
};

conf::item<ssize_t>
max_limit
{
	{ "name",     "ircd.federation.missing_events.limit.max" },
	{ "default",  256L                                       },
};

conf::item<ssize_t>
min_limit
{
	{ "name",     "ircd.federation.missing_events.limit.min" },
	{ "default",  1L                                         },
};

conf::item<size_t>
flush_hiwat
{
	{ "name",     "ircd.federation.missing_events.flush.hiwat" },
	{ "default",  long(16_KiB)                                 },
};

m::resource::response
get__missing_events(client &client,
                    const m::resource::request &request)
{
	if(request.parv.size() < 1)
		throw m::NEED_MORE_PARAMS
		{
			"room_id path parameter required"
		};

	m::room::id::buf room_id
	{
		url::decode(room_id, request.parv[0])
	};

	if(m::room::server_acl::enable_read && !m::room::server_acl::check(room_id, request.node_id))
		throw m::ACCESS_DENIED
		{
			"You are not permitted by the room's server access control list."
		};

	ssize_t limit
	{
		std::clamp
		(
			request.get("limit", 10L),  // protocol spec default
			ssize_t(min_limit),
			ssize_t(max_limit)
		)
	};

	const auto min_depth
	{
		request["min_depth"]?
			lex_cast<uint64_t>(request["min_depth"]):
			0
	};

	const json::array &earliest
	{
		request["earliest_events"]
	};

	const json::array &latest
	{
		request["latest_events"]
	};

	static const auto in_array{[]
	(const json::array &array, const auto &event_id)
	{
		return end(array) != std::find_if(begin(array), end(array), [&event_id]
		(const json::string &_event_id)
		{
			return event_id == _event_id;
		});
	}};

	m::resource::response::chunked response
	{
		client, http::OK
	};

	json::stack out
	{
		response.buf, response.flusher(), size_t(flush_hiwat)
	};

	json::stack::object top{out};
	json::stack::array events
	{
		top, "events"
	};

	std::deque<std::string> queue;
	const auto add_queue{[&limit, &queue, &earliest]
	(const m::event::id &event_id) -> bool
	{
		if(in_array(earliest, event_id))
			return true;

		if(end(queue) != std::find(begin(queue), end(queue), event_id))
			return true;

		if(--limit < 0)
			return false;

		queue.emplace_back(std::string{event_id});
		return true;
	}};

	for(const json::string event_id : latest)
		add_queue(event_id);

	m::event::fetch event;
	while(!queue.empty())
	{
		const auto &event_id
		{
			queue.front()
		};

		const unwind pop{[&queue]
		{
			queue.pop_front();
		}};

		bool ok(true);
		if(!seek(std::nothrow, event, event_id))
			ok = false;

		if(ok && !visible(event, request.node_id))
			ok = false;

		if(!ok)
			log::dwarning
			{
				m::log, "Failed to divulge missing %s in %s to '%s' queue:%zu limit:%ld",
				string_view{event_id},
				string_view{room_id},
				request.node_id,
				queue.size(),
				limit,
			};

		if(!ok)
			continue;

		events.append(event);
		const m::event::prev prev(event);
		for(size_t i(0); i < prev.prev_events_count(); ++i)
			if(!add_queue(prev.prev_event(i)))
				break;
	}

	return response;
}
