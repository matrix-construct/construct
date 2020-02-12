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

ircd::mapi::header
IRCD_MODULE
{
	"Client 11.14 :Server Side Search"
};

ircd::resource
search_resource
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

static void
handle_room_events(client &client,
                   const resource::request &request,
                   const json::object &,
                   json::stack::object &);

static resource::response
post__search(client &client, const resource::request &request);

resource::method
post_method
{
	search_resource, "POST", post__search,
	{
		post_method.REQUIRES_AUTH
	}
};

resource::response
post__search(client &client, const resource::request &request)
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

	handle_room_events(client, request, search_categories, result_categories);
	return std::move(response);
}

void
handle_room_events(client &client,
                   const resource::request &request,
                   const json::object &search_categories,
                   json::stack::object &result_categories)
try
{
	if(!search_categories.has("room_events"))
		return;

	const m::search::room_events room_events
	{
		search_categories["room_events"]
	};

	json::stack::object room_events_result
	{
		result_categories, "room_events"
	};

	json::stack::array results
	{
		room_events_result, "results"
	};

	const json::string &search_term
	{
		at<"search_term"_>(room_events)
	};

	log::debug
	{
		//TODO: search::log
		m::log, "Search [%s] keys:%s order_by:%s inc_state:%b user:%s",
		search_term,
		json::get<"keys"_>(room_events),
		json::get<"order_by"_>(room_events),
		json::get<"include_state"_>(room_events),
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
		//TODO: search::log
		m::log, "Search error :%s", e.what()
	};
}
