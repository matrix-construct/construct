// Matrix Construct
//
// Copyright (C) Matrix Construct Developers, Authors & Contributors
// Copyright (C) 2016-2018 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

namespace ircd::m::search
{
	static void handle_room_events(client &, const resource::request &, const json::object &, json::stack::object &);
	static resource::response search_post_handle(client &, const resource::request &);
	extern resource::method search_post;
	extern resource search_resource;
	extern log::log log;
}

ircd::mapi::header
IRCD_MODULE
{
	"Client 11.14 :Server Side Search"
};

decltype(ircd::m::search::log)
ircd::m::search::log
{
	"m.search"
};

decltype(ircd::m::search::search_resource)
ircd::m::search::search_resource
{
	"/_matrix/client/r0/search",
	{
		"(11.14.1) The search API allows clients to perform full text search"
		" across events in all rooms that the user has been in, including"
		" those that they have left. Only events that the user is allowed to"
		" see will be searched, e.g. it won't include events in rooms that"
		" happened after you left."
	}
};

decltype(ircd::m::search::search_post)
ircd::m::search::search_post
{
	search_resource, "POST", search_post_handle,
	{
		search_post.REQUIRES_AUTH
	}
};

ircd::m::resource::response
ircd::m::search::search_post_handle(client &client,
                                    const resource::request &request)
{
	const auto &batch
	{
		request.query["next_batch"]
	};

	const json::object &search_categories
	{
		request["search_categories"]
	};

	resource::response::chunked response
	{
		client, http::OK
	};

	json::stack out
	{
		response.buf, response.flusher()
	};

	json::stack::object top
	{
		out
	};

	json::stack::object result_categories
	{
		top, "search_categories"
	};

	if(search_categories.has("room_events"))
	{
		json::stack::object room_events_result
		{
			result_categories, "room_events"
		};

		handle_room_events(client, request, search_categories, room_events_result);
	}

	return std::move(response);
}

void
ircd::m::search::handle_room_events(client &client,
                                    const resource::request &request,
                                    const json::object &search_categories,
                                    json::stack::object &room_events_result)
try
{
	const m::search::room_events room_events
	{
		search_categories["room_events"]
	};

	const json::string &search_term
	{
		at<"search_term"_>(room_events)
	};

	const m::room_event_filter filter
	{
		json::get<"filter"_>(room_events)
	};

	const json::array rooms
	{
		json::get<"rooms"_>(filter)
	};

	log::debug
	{
		log, "Query '%s' by %s keys:%s order_by:%s inc_state:%b",
		search_term,
		string_view{request.user_id},
		json::get<"keys"_>(room_events),
		json::get<"order_by"_>(room_events),
		json::get<"include_state"_>(room_events),
	};

	json::stack::array results
	{
		room_events_result, "results"
	};

	long count(0);
	{
		json::stack::object result
		{
			results
		};

		json::stack::member
		{
			result, "rank", json::value(0L)
		};

		json::stack::object result_event
		{
			result, "result"
		};

		//m::event::append::opts opts;
		//m::append(result_event, event, opts);
	}
	results.~array();

	json::stack::member
	{
		room_events_result, "count", json::value(count)
	};

	json::stack::array
	{
		room_events_result, "highlights"
	};

	json::stack::object
	{
		room_events_result, "state"
	};
}
catch(const std::system_error &)
{
	throw;
}
catch(const std::exception &e)
{
	log::error
	{
		log, "search :%s",
		e.what()
	};
}
