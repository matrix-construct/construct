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

conf::item<size_t>
max_limit
{
	{ "name",     "ircd.federation.missing_events.max_limit" },
	{ "default",  128L                                       }
};

conf::item<size_t>
max_goose
{
	{ "name",     "ircd.federation.missing_events.max_goose" },
	{ "default",  512L                                       }
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
		url::decode(request.parv[0], room_id)
	};

	const auto limit
	{
		request["limit"]?
			std::min(lex_cast<size_t>(request["limit"]), size_t(max_limit)):
			size_t(max_limit)
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
		request["latest"]
	};

	const auto in_latest
	{
		[&latest](const auto &event_id)
		{
			return end(latest) != std::find_if(begin(latest), end(latest), [&event_id]
			(const auto &latest)
			{
				return event_id == unquote(latest);
			});
		}
	};

	std::vector<std::string> ret;
	ret.reserve(limit);
	size_t goose{0};
	for(const auto &event_id : earliest) try
	{
		m::room::messages it
		{
			room_id, unquote(event_id)
		};

		for(; it && ret.size() < limit && goose < size_t(max_goose); ++it, ++goose)
		{
			const m::event &event{*it};
			if(!visible(event, request.node_id))
				continue;

			ret.emplace_back(json::strung{event});
			if(in_latest(at<"event_id"_>(event)))
				break;
		}
	}
	catch(const std::exception &e)
	{
		log::derror
		{
			"Request from %s for earliest missing %s :%s",
			string(remote(client)),
			unquote(event_id),
			e.what()
		};
	}

	return resource::response
	{
		client, json::members
		{
			{ "events", json::strung { ret.data(), ret.data() + ret.size() } }
		}
	};
}
