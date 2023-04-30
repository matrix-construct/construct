// Matrix Construct
//
// Copyright (C) Matrix Construct Developers, Authors & Contributors
// Copyright (C) 2016-2019 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

[[gnu::visibility("hidden")]]
decltype(ircd::m::event::append::log)
ircd::m::event::append::log
{
	"m.event.append"
};

[[gnu::visibility("hidden")]]
decltype(ircd::m::event::append::info)
ircd::m::event::append::info
{
	{ "name",     "ircd.m.event.append.info" },
	{ "default",  false                      },
	{ "persist",  false                      },
};

[[gnu::visibility("hidden")]]
decltype(ircd::m::event::append::exclude_types)
ircd::m::event::append::exclude_types
{
	{ "name",     "ircd.m.event.append.exclude.types" },
	{ "default",  "org.matrix.dummy_event"            },
};

/// Default event property mask of keys which we strip from the event sent
/// to the client. This mask is applied only if the caller of event::append{}
/// did not supply their mask to apply. It is also inferior to the user's
/// filter if supplied.
[[gnu::visibility("hidden")]]
decltype(ircd::m::event::append::exclude_keys)
ircd::m::event::append::exclude_keys
{
	"auth_events",
	"hashes",
	"membership",
	"origin",
	"prev_state",
	"signatures",
};

[[gnu::visibility("hidden")]]
decltype(ircd::m::event::append::default_keys)
ircd::m::event::append::default_keys
{
	event::append::exclude_keys
};

bool
ircd::m::event::append::object(json::stack::array &array,
                               const event &event,
                               const opts &opts)
{
	assert(array.s);
	json::stack::checkpoint cp
	{
		*array.s
	};

	json::stack::object _object
	{
		array
	};

	const bool ret
	{
		members(_object, event, opts)
	};

	cp.committing(ret);
	return ret;
}

bool
ircd::m::event::append::members(json::stack::object &out,
                                const event &event,
                                const opts &opts)
{
	// Assertions that the event being appended has some required fields. This
	// is a central butt-end test of data coming through the system to here.
	assert(event.event_id);
	assert(defined(json::get<"type"_>(event)));
	assert(defined(json::get<"sender"_>(event)));
	//assert(json::get<"origin_server_ts"_>(event));
	//assert(json::get<"origin_server_ts"_>(event) != json::undefined_number);
	if constexpr(RB_DEBUG_LEVEL)
	{
		if(unlikely(!defined(json::get<"type"_>(event))))
			return false;

		if(unlikely(!defined(json::get<"sender"_>(event))))
			return false;
	}

	if(opts.event_filter && !m::match(*opts.event_filter, event))
		return false;

	if(is_excluded(event, opts))
		return false;

	if(is_invisible(event, opts))
		return false;

	if(is_redacted(event, opts))
		return false;

	if(is_ignored(event, opts))
		return false;

	// For v3+ events
	if(!json::get<"event_id"_>(event))
		json::stack::member
		{
			out, "event_id", event.event_id
		};

	// Get the list of properties to send to the client so we can strip
	// the remaining and save b/w
	// TODO: m::filter
	const event::keys &keys
	{
		opts.keys?
			*opts.keys:
			default_keys
	};

	// Append the event members
	for_each(event, [&keys, &out]
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
			out, key, val
		};

		return true;
	});

	_unsigned(out, event, opts);

	if(unlikely(info))
		log::info
		{
			log, "%s %s idx:%lu in %s depth:%ld txnid:%s %s,%s",
			string_view{opts.user_id},
			string_view{event.event_id},
			opts.event_idx,
			json::get<"room_id"_>(event),
			json::get<"depth"_>(event),
			opts.client_txnid,
			json::get<"type"_>(event),
			json::get<"state_key"_>(event),
		};

	return true;
}

void
ircd::m::event::append::_unsigned(json::stack::object &out,
                                  const event &event,
                                  const opts &opts)
{
	json::stack::object object
	{
		out, "unsigned"
	};

	_age(object, event, opts);
	_txnid(object, event, opts);
	_relations(object, event, opts);
	if(defined(json::get<"state_key"_>(event)))
		_prev_state(object, event, opts);
}

void
ircd::m::event::append::_prev_state(json::stack::object &out,
                                    const event &event,
                                    const opts &opts)
{
	assert(defined(json::get<"state_key"_>(event)));

	const bool query_prev_state
	{
		true
		&& opts.event_idx
		&& opts.query_prev_state
	};

	const auto prev_state_idx
	{
		query_prev_state?
			room::state::prev(opts.event_idx):
			0UL
	};

	if(prev_state_idx)
	{
		m::get(std::nothrow, prev_state_idx, "content", [&out]
		(const json::object &content)
		{
			json::stack::member
			{
				out, "prev_content", content
			};
		});

		const auto replaces_state_id
		{
			m::event_id(std::nothrow, prev_state_idx)
		};

		json::stack::member
		{
			out, "replaces_state", json::value
			{
				replaces_state_id?
					string_view{replaces_state_id}:
					string_view{}
			}
		};
	}
}

void
ircd::m::event::append::_txnid(json::stack::object &out,
                               const event &event,
                               const opts &opts)
{
	const bool sender_is_user
	{
		json::get<"sender"_>(event) == opts.user_id
	};

	const bool query_txnid
	{
		true
		&& !opts.client_txnid
		&& opts.query_txnid
		&& opts.user_room_id
		&& sender_is_user
	};

	const auto txnid_idx
	{
		query_txnid?
			m::room(opts.user_room_id).get(std::nothrow, "ircd.client.txnid", event.event_id):
			0UL
	};

	if constexpr(RB_DEBUG_LEVEL)
	{
		const bool missing_txnid
		{
			true
			&& !opts.client_txnid
			&& !txnid_idx
			&& sender_is_user
			&& opts.query_txnid
		};

		if(unlikely(missing_txnid))
			log::dwarning
			{
				log, "Could not find transaction_id for %s from %s in %s",
				string_view{event.event_id},
				json::get<"sender"_>(event),
				json::get<"room_id"_>(event)
			};
	}

	if(opts.client_txnid)
		json::stack::member
		{
			out, "transaction_id", opts.client_txnid
		};
	else if(txnid_idx)
		m::get(std::nothrow, txnid_idx, "content", [&out]
		(const json::object &content)
		{
			json::stack::member
			{
				out, "transaction_id", content.get("transaction_id")
			};
		});
}

void
ircd::m::event::append::_age(json::stack::object &out,
                             const event &event,
                             const opts &opts)
{
	const json::value age
	{
		// When the opts give an explicit age, use it.
		opts.age != std::numeric_limits<long>::min()?
			opts.age:

		// If we have depth information, craft a value based on the
		// distance to the head depth; if this is 0 in riot the event will
		// "stick" at the bottom of the timeline. This may be advantageous
		// in the future but for now we make sure the result is non-zero.
		json::get<"depth"_>(event) >= 0 && opts.room_depth >= 0L?
			(opts.room_depth + 1 - json::get<"depth"_>(event)) + 1:

		// We don't have depth information, so we use the origin_server_ts.
		// It is bad if it conflicts with other appends in the room which
		// did have depth information.
		opts.room_depth < 0 && json::get<"origin_server_ts"_>(event)?
			ircd::time<milliseconds>() - json::get<"origin_server_ts"_>(event):

		// Finally, this special value will eliminate the age altogether
		// during serialization.
		json::undefined_number
	};

	json::stack::member
	{
		out, "age", age
	};
}

void
ircd::m::event::append::_relations(json::stack::object &out,
                                   const event &event,
                                   const opts &opts)
{
	assert(out.s);
	json::stack::checkpoint cp
	{
		*out.s, false
	};

	json::stack::object object
	{
		out, "m.relations"
	};

	bool commit
	{
		cp.committing()
	};

	if(opts.bundle_all || opts.bundle_replace)
		commit |= bundle_replace(object, event, opts);

	cp.committing(commit);
}

bool
ircd::m::event::append::bundle_replace(json::stack::object &out,
                                       const event &event,
                                       const opts &opts)
{
	const m::replaced replaced
	{
		opts.event_idx, m::replaced::latest
	};

	const event::idx &replace_idx
	{
		replaced
	};

	if(likely(!replace_idx))
		return false;

	const m::event::fetch replace
	{
		std::nothrow, replace_idx
	};

	if(unlikely(!replace.valid))
		return false;

	json::stack::object object
	{
		out, "m.replace"
	};

	object.append(replace);
	return true;
}

bool
ircd::m::event::append::is_excluded(const event &event,
                                    const opts &opts)
const
{
	const auto &not_types
	{
		exclude_types
	};

	const bool ret
	{
		true
		&& !opts.event_filter
		&& token_exists(not_types, ' ', json::get<"type"_>(event))
	};

	if(ret)
		log::debug
		{
			log, "Not sending event %s because type '%s' excluded by configuration.",
			string_view{event.event_id},
			json::get<"type"_>(event),
		};

	return ret;
}

bool
ircd::m::event::append::is_invisible(const event &event,
                                     const opts &opts)
const
{
	const bool ret
	{
		true
		&& opts.query_visible
		&& opts.user_id
		&& !visible(event, opts.user_id)
	};

	if(ret)
		log::debug
		{
			log, "Not sending event %s because not visible to %s.",
			string_view{event.event_id},
			string_view{opts.user_id},
		};

	return ret;
}

bool
ircd::m::event::append::is_redacted(const event &event,
                                    const opts &opts)
const
{
	const bool ret
	{
		true
		&& opts.event_idx
		&& opts.query_redacted
		&& !defined(json::get<"state_key"_>(event))
		&& opts.room_depth > json::get<"depth"_>(event)
		&& m::redacted(opts.event_idx)
	};

	if(ret)
		log::debug
		{
			log, "Not sending event %s because redacted.",
			string_view{event.event_id},
		};

	return ret;
}

bool
ircd::m::event::append::is_ignored(const event &event,
                                   const opts &opts)
const
{
	const bool check_ignores
	{
		true
		&& !defined(json::get<"state_key"_>(event))
		&& opts.user_id
		&& opts.user_room_id
		&& opts.user_id != json::get<"sender"_>(event)
	};

	if(!check_ignores)
		return false;

	const m::user::ignores ignores
	{
		opts.user_id
	};

	if(ignores.enforce("events") && ignores.has(json::get<"sender"_>(event)))
	{
		log::debug
		{
			log, "Not sending event %s because %s is ignored by %s",
			string_view{event.event_id},
			json::get<"sender"_>(event),
			string_view{opts.user_id}
		};

		return true;
	}

	return false;
}
