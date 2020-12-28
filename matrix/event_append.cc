// Matrix Construct
//
// Copyright (C) Matrix Construct Developers, Authors & Contributors
// Copyright (C) 2016-2019 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

namespace ircd::m
{
	extern const event::keys::exclude event_append_exclude_keys;
	extern const event::keys event_append_default_keys;
	extern conf::item<bool> event_append_info;
	extern log::log event_append_log;
}

decltype(ircd::m::event_append_log)
ircd::m::event_append_log
{
	"m.event.append"
};

decltype(ircd::m::event_append_info)
ircd::m::event_append_info
{
	{ "name",     "ircd.m.event.append.info" },
	{ "default",  false                      },
	{ "persist",  false                      },
};

/// Default event property mask of keys which we strip from the event sent
/// to the client. This mask is applied only if the caller of event::append{}
/// did not supply their mask to apply. It is also inferior to the user's
/// filter if supplied.
decltype(ircd::m::event_append_exclude_keys)
ircd::m::event_append_exclude_keys
{
	"auth_events",
	"hashes",
	"membership",
	"origin",
	"prev_state",
	"signatures",
};

decltype(ircd::m::event_append_default_keys)
ircd::m::event_append_default_keys
{
	event_append_exclude_keys
};

ircd::m::event::append::append(json::stack::array &array,
                               const event &event_,
                               const opts &opts)
:boolean{[&]
{
	assert(array.s);
	json::stack::checkpoint cp
	{
		*array.s
	};

	json::stack::object object
	{
		array
	};

	const bool ret
	{
		append
		{
			object, event_, opts
		}
	};

	cp.committing(ret);
	return ret;
}}
{
}

ircd::m::event::append::append(json::stack::object &object,
                               const event &event,
                               const opts &opts)
:boolean{[&]
{
	// Assertions that the event being appended has some required fields. This
	// is a central butt-end test of data coming through the system to here.
	assert(event.event_id);
	assert(defined(json::get<"type"_>(event)));
	assert(defined(json::get<"sender"_>(event)));
	//assert(json::get<"origin_server_ts"_>(event));
	//assert(json::get<"origin_server_ts"_>(event) != json::undefined_number);
	#if defined(RB_DEBUG)
	if(unlikely(!defined(json::get<"type"_>(event))))
		return false;

	if(unlikely(!defined(json::get<"sender"_>(event))))
		return false;
	#endif

	if(opts.event_filter && !m::match(*opts.event_filter, event))
		return false;

	if(opts.query_visible && opts.user_id && !visible(event, *opts.user_id))
	{
		log::debug
		{
			log, "Not sending event %s because not visible to %s.",
			string_view{event.event_id},
			string_view{*opts.user_id},
		};

		return false;
	}

	const bool has_event_idx
	{
		opts.event_idx && *opts.event_idx
	};

	const bool is_state
	{
		defined(json::get<"state_key"_>(event))
	};

	const bool query_redacted
	{
		has_event_idx &&
		opts.query_redacted &&
		!is_state &&
		(!opts.room_depth || *opts.room_depth > json::get<"depth"_>(event))
	};

	if(query_redacted && m::redacted(*opts.event_idx))
	{
		log::debug
		{
			log, "Not sending event %s because redacted.",
			string_view{event.event_id},
		};

		return false;
	}

	const bool has_user
	{
		opts.user_id && opts.user_room
	};

	const bool check_ignores
	{
		has_user && !is_state
	};

	if(check_ignores && *opts.user_id != json::get<"sender"_>(event))
	{
		const m::user::ignores ignores
		{
			*opts.user_id
		};

		if(ignores.enforce("events") && ignores.has(json::get<"sender"_>(event)))
		{
			log::debug
			{
				log, "Not sending event %s because %s is ignored by %s",
				string_view{event.event_id},
				json::get<"sender"_>(event),
				string_view{*opts.user_id}
			};

			return false;
		}
	}

	const bool sender_is_user
	{
		has_user && json::get<"sender"_>(event) == *opts.user_id
	};

	const bool has_client_txnid
	{
		opts.client_txnid && *opts.client_txnid
	};

	const auto txnid_idx
	{
		!has_client_txnid && sender_is_user && opts.query_txnid?
			opts.user_room->get(std::nothrow, "ircd.client.txnid", event.event_id):
			0UL
	};

	#if defined(RB_DEBUG) && 0
	if(!has_client_txnid && !txnid_idx && sender_is_user && opts.query_txnid)
		log::dwarning
		{
			log, "Could not find transaction_id for %s from %s in %s",
			string_view{event.event_id},
			json::get<"sender"_>(event),
			json::get<"room_id"_>(event)
		};
	#endif

	if(!json::get<"event_id"_>(event))
		json::stack::member
		{
			object, "event_id", event.event_id
		};

	const bool query_prev_state
	{
		has_event_idx && opts.query_prev_state && is_state
	};

	if(query_prev_state)
	{
		const auto prev_idx
		{
			room::state::prev(*opts.event_idx)
		};

		m::get(std::nothrow, prev_idx, "content", [&object]
		(const json::object &content)
		{
			json::stack::member
			{
				object, "prev_content", content
			};
		});
	}

	// Get the list of properties to send to the client so we can strip
	// the remaining and save b/w
	// TODO: m::filter
	const event::keys &keys
	{
		opts.keys?
			*opts.keys:
			event_append_default_keys
	};

	// Append the event members
	for_each(event, [&keys, &object]
	(const auto &key, const auto &val_)
	{
		if(!keys.has(key) && key != "redacts"_sv)
			return true;

		const json::value val
		{
			val_
		};

		if(!defined(val))
			return true;

		json::stack::member
		{
			object, key, val
		};

		return true;
	});

	json::stack::object unsigned_
	{
		object, "unsigned"
	};

	const json::value age
	{
		// When the opts give an explicit age, use it.
		opts.age != std::numeric_limits<long>::min()?
			opts.age:

		// If we have depth information, craft a value based on the
		// distance to the head depth; if this is 0 in riot the event will
		// "stick" at the bottom of the timeline. This may be advantageous
		// in the future but for now we make sure the result is non-zero.
		json::get<"depth"_>(event) >= 0 && opts.room_depth && *opts.room_depth >= 0L?
			(*opts.room_depth + 1 - json::get<"depth"_>(event)) + 1:

		// We don't have depth information, so we use the origin_server_ts.
		// It is bad if it conflicts with other appends in the room which
		// did have depth information.
		!opts.room_depth && json::get<"origin_server_ts"_>(event)?
			ircd::time<milliseconds>() - json::get<"origin_server_ts"_>(event):

		// Finally, this special value will eliminate the age altogether
		// during serialization.
		json::undefined_number
	};

	json::stack::member
	{
		unsigned_, "age", age
	};

	if(has_client_txnid)
		json::stack::member
		{
			unsigned_, "transaction_id", *opts.client_txnid
		};

	if(txnid_idx)
		m::get(std::nothrow, txnid_idx, "content", [&unsigned_]
		(const json::object &content)
		{
			json::stack::member
			{
				unsigned_, "transaction_id", unquote(content.get("transaction_id"))
			};
		});

	if(unlikely(event_append_info))
		log::info
		{
			event_append_log, "%s %s idx:%lu in %s depth:%ld txnid:%s idx:%lu age:%ld %s,%s",
			opts.user_id? string_view{*opts.user_id} : string_view{},
			string_view{event.event_id},
			opts.event_idx? *opts.event_idx : 0UL,
			json::get<"room_id"_>(event),
			json::get<"depth"_>(event),
			has_client_txnid? *opts.client_txnid : string_view{},
			txnid_idx,
			int64_t(age),
			json::get<"type"_>(event),
			json::get<"state_key"_>(event),
		};

	return true;
}}
{
}
