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

resource
get_missing_events_resource
{
	"/_matrix/federation/v1/get_missing_events/",
	{
		"Federation (undocumented) missing events handler",
		resource::DIRECTORY,
	}
};

static resource::response get__missing_events(client &, const resource::request &);

resource::method
method_get
{
	get_missing_events_resource, "GET", get__missing_events,
	{
		method_get.VERIFY_ORIGIN
	}
};

resource::method
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
	{ "name",     "ircd.federation.missing_events.max_limit" },
	{ "default",  128L                                       }
};

conf::item<size_t>
flush_hiwat
{
	{ "name",     "ircd.federation.missing_events.flush.hiwat" },
	{ "default",  long(16_KiB)                                 },
};

resource::response
get__missing_events(client &client,
                    const resource::request &request)
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

	ssize_t limit
	{
		request["limit"]?
			std::min(lex_cast<ssize_t>(request["limit"]), ssize_t(max_limit)):
			ssize_t(10) // default limit (protocol spec)
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

	const auto in_earliest{[&earliest](const auto &event_id)
	{
		return end(earliest) != std::find_if(begin(earliest), end(earliest), [&event_id]
		(const auto &event_id_)
		{
			return event_id == unquote(event_id_);
		});
	}};

	resource::response::chunked response
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
	const auto add_queue{[&limit, &queue, &in_earliest]
	(const m::event::id &event_id) -> bool
	{
		if(in_earliest(event_id))
			return true;

		if(end(queue) != std::find(begin(queue), end(queue), event_id))
			return true;

		if(--limit < 0)
			return false;

		queue.emplace_back(std::string{event_id});
		return true;
	}};

	for(const auto &event_id : latest)
		add_queue(unquote(event_id));

	m::event::fetch event;
	while(!queue.empty())
	{
		const auto &event_id{queue.front()};
		const unwind pop{[&queue]
		{
			queue.pop_front();
		}};

		if(!seek(event, event_id, std::nothrow))
			continue;

		if(!visible(event, request.node_id))
			continue;

		events.append(event);
		for(const json::array &prev : json::get<"prev_events"_>(event))
			if(!add_queue(unquote(prev.at(0))))
				break;
	}

	return response;
}
