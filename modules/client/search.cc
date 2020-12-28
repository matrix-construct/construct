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
	static bool handle_result(result &, const query &);
	static bool handle_content(result &, const query &, const json::object &);
	static bool query_all_rooms(result &, const query &);
	static bool query_room(result &, const query &, const room::id &);
	static bool query_rooms(result &, const query &);
	static void handle_room_events(client &, const resource::request &, const json::object &, json::stack::object &);
	static resource::response search_post_handle(client &, const resource::request &);

	extern conf::item<bool> count_total;
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
		search_post.REQUIRES_AUTH,

		// Some queries can take a really long time, especially under
		// development. We don't need the default request timer getting
		// in the way for now.
		60s,
	}
};

decltype(ircd::m::search::count_total)
ircd::m::search::count_total
{
	{ "name",     "ircd.m.search.count.total" },
	{ "default",  false                       },
};

ircd::m::resource::response
ircd::m::search::search_post_handle(client &client,
                                    const resource::request &request)
{
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

	const m::room_event_filter room_event_filter
	{
		json::get<"filter"_>(room_events)
	};

	const json::object &event_context
	{
		json::get<"event_context"_>(room_events)
	};

	// Spec sez default is 5. Reference client does not make any use of
	// result context if provided.
	const ushort context_default
	{
		0
	};

	const search::query query
	{
		request.user_id,
		request.query.get<size_t>("next_batch", 0UL),
		room_events,
		room_event_filter,
		at<"search_term"_>(room_events),
		size_t(json::get<"limit"_>(query.filter))?: -1UL,
		event_context.get("before_limit", context_default),
		event_context.get("after_limit", context_default),
	};

	log::logf
	{
		log, log::DEBUG,
		"Query '%s' by %s batch:%ld order_by:%s inc_state:%b rooms:%zu limit:%zu",
		query.search_term,
		string_view{query.user_id},
		query.batch,
		json::get<"order_by"_>(query.room_events),
		json::get<"include_state"_>(query.room_events),
		json::get<"rooms"_>(query.filter).size(),
		query.limit,
	};

	search::result result
	{
		room_events_result.s
	};

	const bool finished
	{
		query_rooms(result, query)
	};

	// Spec sez this is total results, but riot doesn't use it. Counting total
	// results is very expensive right now, so we'll just report the count we
	// have for now...
	json::stack::member
	{
		room_events_result, "count", json::value
		{
			long(result.count + !finished)
		}
	};

	//TODO: XXX
	json::stack::array
	{
		room_events_result, "highlights"
	};

	//TODO: XXX
	json::stack::object
	{
		room_events_result, "state"
	};

	if(!finished)
		json::stack::member
		{
			room_events_result, "next_batch", json::value
			{
				lex_cast(result.skipped + result.checked), json::STRING
			}
		};

	char tmbuf[48];
	log::logf
	{
		log, log::DEBUG,
		"Result '%s' by %s batch[%ld -> %ld] count:%lu append:%lu match:%lu check:%lu skip:%lu in %s",
		query.search_term,
		string_view{query.user_id},
		query.batch,
		result.event_idx,
		result.count,
		result.appends,
		result.matched,
		result.checked,
		result.skipped,
		result.elapsed.pretty(tmbuf),
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

bool
ircd::m::search::query_rooms(result &result,
                             const query &query)
{
	const json::array rooms
	{
		json::get<"rooms"_>(query.filter)
	};

	json::stack::array results
	{
		*result.out, "results"
	};

	if(rooms.empty())
		return query_all_rooms(result, query);

	for(const json::string room_id : rooms)
		if(!query_room(result, query, room_id))
			return false;

	return true;
}

bool
ircd::m::search::query_room(result &result,
                            const query &query,
                            const room::id &room_id)
{
	const m::room room
	{
		room_id
	};

	if(!visible(room, query.user_id))
		throw m::ACCESS_DENIED
		{
			"You are not permitted to view %s",
			string_view{room_id},
		};

	const m::room::content content
	{
		room
	};

	return content.for_each([&result, &query]
	(const json::object &content, const auto &depth, const auto &event_idx)
	{
		result.event_idx = event_idx;
		return handle_content(result, query, content);
	});
}

bool
ircd::m::search::query_all_rooms(result &result,
                                 const query &query)
{
	if(!is_oper(query.user_id))
		throw m::ACCESS_DENIED
		{
			"You are not an operator."
		};

	return m::events::content::for_each([&result, &query]
	(const auto &event_idx, const json::object &content)
	{
		result.event_idx = event_idx;
		return handle_content(result, query, content);
	});
}

bool
ircd::m::search::handle_content(result &result,
                                const query &query,
                                const json::object &content)
try
{
	if(result.skipped < query.batch)
	{
		++result.skipped;
		return true;
	}

	const json::string body
	{
		content["body"]
	};

	const bool match
	{
		has(body, query.search_term)
	};

	const bool handled
	{
		match && handle_result(result, query)
	};

	result.checked += 1;
	result.matched += match;
	result.count += handled;
	return result.count < query.limit;
}
catch(const ctx::interrupted &e)
{
	log::dwarning
	{
		log, "Query handling '%s' by '%s' event_idx:%lu :%s",
		query.search_term,
		string_view{query.user_id},
		result.event_idx,
		e.what(),
	};

	throw;
}
catch(const std::system_error &e)
{
	log::derror
	{
		log, "Query handling for '%s' by '%s' event_idx:%lu :%s",
		query.search_term,
		string_view{query.user_id},
		result.event_idx,
		e.what(),
	};

	throw;
}
catch(const std::exception &e)
{
	log::error
	{
		log, "Query handling for '%s' by '%s' event_idx:%lu :%s",
		query.search_term,
		string_view{query.user_id},
		result.event_idx,
		e.what(),
	};

	return true;
}

bool
ircd::m::search::handle_result(result &result,
                               const query &query)
try
{
	const m::event_filter event_filter
	{
		query.filter
	};

	const m::event::fetch event
	{
		result.event_idx
	};

	assert(result.out);
	json::stack::checkpoint cp
	{
		*result.out, false
	};

	json::stack::object object
	{
		*result.out
	};

	json::stack::member
	{
		object, "rank", json::value(result.rank)
	};

	m::event::append::opts opts;
	opts.event_idx = &result.event_idx;
	opts.user_id = &query.user_id;
	opts.event_filter = &event_filter;
	opts.query_prev_state = false;
	opts.query_visible = true;
	bool ret{false};
	{
		json::stack::object result_event
		{
			object, "result"
		};

		ret = event::append(result_event, event, opts);
		result.appends += ret;
		cp.committing(ret);
	}

	if(!query.before_limit && !query.after_limit)
		return ret;

	const m::room room
	{
		json::get<"room_id"_>(event)
	};

	m::room::events it
	{
		room
	};

	json::stack::object result_context
	{
		object, "context"
	};

	size_t before(0);
	if(likely(!it.seek(result.event_idx)))
	{
		json::stack::array events_before
		{
			result_context, "events_before"
		};

		for(--it; it && before < query.before_limit; ++before, --it)
		{
			const event::idx event_idx
			{
				it.event_idx()
			};

			opts.event_idx = &event_idx;
			result.appends += event::append(events_before, event, opts);
		}
	}

	size_t after(0);
	if(likely(it.seek(result.event_idx)))
	{
		json::stack::array events_after
		{
			result_context, "events_after"
		};

		for(++it; it && after < query.after_limit; ++after, ++it)
		{
			const event::idx event_idx
			{
				it.event_idx()
			};

			opts.event_idx = &event_idx;
			result.appends += event::append(events_after, event, opts);
		}
	}

	return ret;
}
catch(const ctx::interrupted &)
{
	throw;
}
catch(const std::system_error &e)
{
	throw;
}
catch(const std::exception &e)
{
	log::error
	{
		log, "Result handling for '%s' by '%s' event_idx:%lu :%s",
		query.search_term,
		string_view{query.user_id},
		result.event_idx,
		e.what(),
	};

	return false;
}
