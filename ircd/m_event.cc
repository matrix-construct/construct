// Matrix Construct
//
// Copyright (C) Matrix Construct Developers, Authors & Contributors
// Copyright (C) 2016-2018 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

///////////////////////////////////////////////////////////////////////////////
//
// event/pretty.h
//

std::string
ircd::m::pretty(const event &event)
{
	std::string ret;
	std::stringstream s;
	pubsetbuf(s, ret, 4096);
	pretty(s, event);
	resizebuf(s, ret);
	return ret;
}

std::ostream &
ircd::m::pretty(std::ostream &s,
                const event &event)
{
	const auto out{[&s]
	(const string_view &key, auto&& val)
	{
		if(defined(json::value(val)))
			s << std::setw(16) << std::right << key << " :" << val << std::endl;
	}};

	const string_view top_keys[]
	{
		"origin",
		"event_id",
		"room_id",
		"sender",
		"type",
		"depth",
		"state_key",
		"redacts",
	};

	json::for_each(event, top_keys, out);

	const auto &ts{json::get<"origin_server_ts"_>(event)};
	{
		thread_local char buf[128];
		s << std::setw(16) << std::right << "origin_server_ts" << " :"
		  << timef(buf, ts / 1000L, ircd::localtime)
		  << " (" << ts << ")"
		  << std::endl;
	}

	const json::object &contents{json::get<"content"_>(event)};
	if(!contents.empty())
		s << std::setw(16) << std::right << "content" << " :"
		  << size(contents) << " keys; "
		  << size(string_view{contents}) << " bytes."
		  << std::endl;

	const auto &hashes{json::get<"hashes"_>(event)};
	for(const auto &hash : hashes)
	{
		s << std::setw(16) << std::right << "[hash]" << " :"
		  << hash.first
		  << " "
		  << unquote(hash.second)
		  << std::endl;
	}

	const auto &signatures{json::get<"signatures"_>(event)};
	for(const auto &signature : signatures)
	{
		s << std::setw(16) << std::right << "[signature]" << " :"
		  << signature.first << " ";

		for(const auto &key : json::object{signature.second})
			s << key.first << " ";

		s << std::endl;
	}

	const m::event::prev prev
	{
		event
	};

	pretty(s, prev);

	if(!contents.empty())
		for(const json::object::member &content : contents)
			s << std::setw(16) << std::right << "[content]" << " :"
			  << std::setw(7) << std::left << reflect(json::type(content.second)) << " "
			  << std::setw(5) << std::right << size(string_view{content.second}) << " bytes "
			  << ':' << content.first
			  << std::endl;

	return s;
}

std::string
ircd::m::pretty_oneline(const event &event,
                        const bool &content_keys)
{
	std::string ret;
	std::stringstream s;
	pubsetbuf(s, ret, 4096);
	pretty_oneline(s, event, content_keys);
	resizebuf(s, ret);
	return ret;
}

std::ostream &
ircd::m::pretty_oneline(std::ostream &s,
                        const event &event,
                        const bool &content_keys)
{
	if(defined(json::get<"room_id"_>(event)))
		s << json::get<"room_id"_>(event) << " ";
	else
		s << "* ";

	if(json::get<"depth"_>(event) != json::undefined_number)
		s << json::get<"depth"_>(event) << " ";
	else
		s << "* ";

	thread_local char sdbuf[48];
	if(json::get<"origin_server_ts"_>(event) != json::undefined_number)
		s << smalldate(sdbuf, json::get<"origin_server_ts"_>(event) / 1000L) << " ";
	else
		s << "* ";

	if(defined(json::get<"origin"_>(event)))
		s << ':' << json::get<"origin"_>(event) << " ";
	else
		s << ":* ";

	if(defined(json::get<"sender"_>(event)))
		s << json::get<"sender"_>(event) << " ";
	else
		s << "* ";

	if(event.event_id)
		s << event.event_id << " ";
	else
		s << "* ";

	const auto &auth_events{json::get<"auth_events"_>(event)};
	s << "A:" << auth_events.count() << " ";

	const auto &prev_events{json::get<"prev_events"_>(event)};
	s << "E:" << prev_events.count() << " ";

	const auto &hashes{json::get<"hashes"_>(event)};
	s << "[ ";
	for(const auto &hash : hashes)
		s << hash.first << " ";
	s << "] ";

	const auto &signatures{json::get<"signatures"_>(event)};
	s << "[ ";
	for(const auto &signature : signatures)
	{
		s << signature.first << "[ ";
		for(const auto &key : json::object{signature.second})
			s << key.first << " ";

		s << "] ";
	}
	s << "] ";

	if(defined(json::get<"type"_>(event)))
		s << json::get<"type"_>(event) << " ";
	else
		s << "* ";

	const auto &state_key
	{
		json::get<"state_key"_>(event)
	};

	if(defined(state_key) && empty(state_key))
		s << "\"\"" << " ";
	else if(defined(state_key))
		s << state_key << " ";
	else
		s << "*" << " ";

	const string_view &membership
	{
		json::get<"type"_>(event) == "m.room.member"?
			m::membership(event):
			"*"_sv
	};

	s << membership << " ";

	if(defined(json::get<"redacts"_>(event)))
		s << json::get<"redacts"_>(event) << " ";
	else
		s << "* ";

	const json::object &contents
	{
		content_keys?
			json::get<"content"_>(event):
			json::object{}
	};

	if(!contents.empty())
	{
		s << "+" << string_view{contents}.size() << " bytes :";
		for(const auto &content : contents)
			s << content.first << " ";
	}

	return s;
}

std::string
ircd::m::pretty_msgline(const event &event)
{
	std::string ret;
	std::stringstream s;
	pubsetbuf(s, ret, 4096);
	pretty_msgline(s, event);
	resizebuf(s, ret);
	return ret;
}

std::ostream &
ircd::m::pretty_msgline(std::ostream &s,
                        const event &event)
{
	s << json::get<"depth"_>(event) << " :";
	s << json::get<"type"_>(event) << " ";
	s << json::get<"sender"_>(event) << " ";
	s << event.event_id << " ";

	const auto &state_key
	{
		json::get<"state_key"_>(event)
	};

	if(defined(state_key) && empty(state_key))
		s << "\"\"" << " ";
	else if(defined(state_key))
		s << state_key << " ";
	else
		s << "*" << " ";

	const json::object &content
	{
		json::get<"content"_>(event)
	};

	switch(hash(json::get<"type"_>(event)))
	{
		case "m.room.message"_:
			s << unquote(content.get("msgtype")) << " ";
			s << unquote(content.get("body")) << " ";
			break;

		default:
			s << string_view{content};
			break;
	}

	return s;
}

std::string
ircd::m::pretty(const event::prev &prev)
{
	std::string ret;
	std::stringstream s;
	pubsetbuf(s, ret, 4096);
	pretty(s, prev);
	resizebuf(s, ret);
	return ret;
}

std::ostream &
ircd::m::pretty(std::ostream &s,
                const event::prev &prev)
{
	for(size_t i(0); i < prev.auth_events_count(); ++i)
	{
		const auto &[event_id, ref_hash]
		{
			prev.auth_events(i)
		};

		s << std::setw(16) << std::right << "[auth event]"
		  << " :" << event_id;

		for(const auto &[algorithm, digest] : ref_hash)
			s << " " << unquote(algorithm)
			  << ": " << unquote(digest);

		s << std::endl;
	}

	for(size_t i(0); i < prev.prev_events_count(); ++i)
	{
		const auto &[event_id, ref_hash]
		{
			prev.prev_events(i)
		};

		s << std::setw(16) << std::right << "[prev_event]"
		  << " :" << event_id;

		for(const auto &[algorithm, digest] : ref_hash)
			s << " " << unquote(algorithm)
			  << ": " << unquote(digest);

		s << std::endl;
	}

	return s;
}

std::ostream &
ircd::m::pretty_oneline(std::ostream &s,
                        const event::prev &prev)
{
	const auto &auth_events{json::get<"auth_events"_>(prev)};
	s << "A[ ";
	for(const json::array auth_event : auth_events)
		s << unquote(auth_event[0]) << " ";
	s << "] ";

	const auto &prev_events{json::get<"prev_events"_>(prev)};
	s << "E[ ";
	for(const json::array prev_event : prev_events)
		s << unquote(prev_event[0]) << " ";
	s << "] ";

	return s;
}

///////////////////////////////////////////////////////////////////////////////
//
// event/append.h
//

void
ircd::m::append(json::stack::array &array,
                const event &event_,
                const event_append_opts &opts)
{
	json::stack::object object
	{
		array
	};

	append(object, event_, opts);
}

void
ircd::m::append(json::stack::object &object,
                const event &event,
                const event_append_opts &opts)
{
	const bool has_event_idx
	{
		opts.event_idx && *opts.event_idx
	};

	const bool has_client_txnid
	{
		opts.client_txnid && *opts.client_txnid
	};

	const bool has_user
	{
		opts.user_id && opts.user_room
	};

	const bool sender_is_user
	{
		has_user && json::get<"sender"_>(event) == *opts.user_id
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

	if(!json::get<"state_key"_>(event) && has_user)
	{
		const m::user::ignores ignores{*opts.user_id};
		if(ignores.enforce("events") && ignores.has(json::get<"sender"_>(event)))
		{
			log::debug
			{
				log, "Not sending event '%s' because '%s' is ignored by '%s'",
				string_view{event.event_id},
				json::get<"sender"_>(event),
				string_view{*opts.user_id}
			};

			return;
		}
	}

	object.append(event);

	if(json::get<"state_key"_>(event) && has_event_idx)
	{
		const auto prev_idx
		{
			room::state::prev(*opts.event_idx)
		};

		if(prev_idx)
			m::get(std::nothrow, prev_idx, "content", [&object]
			(const json::object &content)
			{
				json::stack::member
				{
					object, "prev_content", content
				};
			});
	}

	json::stack::object unsigned_
	{
		object, "unsigned"
	};

	json::stack::member
	{
		unsigned_, "age", json::value
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
		}
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
}

///////////////////////////////////////////////////////////////////////////////
//
// event/conforms.h
//

namespace ircd::m::vm
{
	extern const m::hookfn<eval &> issue_missing_auth;
	extern const m::hookfn<eval &> conform_check_origin;
	extern const m::hookfn<eval &> conform_check_size;
	extern const m::hookfn<eval &> conform_report;
}

/// Check if an eval with a copts structure (indicating this server is
/// creating the event) has an origin set to !my_host().
decltype(ircd::m::vm::conform_check_origin)
ircd::m::vm::conform_check_origin
{
	{
		{ "_site", "vm.conform" }
	},
	[](const m::event &event, eval &eval)
	{
		if(unlikely(eval.copts && !my_host(at<"origin"_>(event))))
			throw error
			{
				fault::GENERAL, "Issuing event for origin: %s", at<"origin"_>(event)
			};
	}
};

/// Check if an event originating from this server exceeds maximum size.
decltype(ircd::m::vm::conform_check_size)
ircd::m::vm::conform_check_size
{
	{
		{ "_site",  "vm.conform"  },
		{ "origin",  my_host()    }
	},
	[](const m::event &event, eval &eval)
	{
		const size_t &event_size
		{
			serialized(event)
		};

		if(event_size > size_t(event::max_size))
			throw m::BAD_JSON
			{
				"Event is %zu bytes which is larger than the maximum %zu bytes",
				event_size,
				size_t(event::max_size)
			};
	}
};

/// Check if an event originating from this server exceeds maximum size.
decltype(ircd::m::vm::conform_report)
ircd::m::vm::conform_report
{
	{
		{ "_site",  "vm.conform"  }
	},
	[](const m::event &event, eval &eval)
	{
		assert(eval.opts);
		auto &opts(*eval.opts);

		// When opts.conformed is set the report is already generated
		if(opts.conformed)
		{
			eval.report = opts.report;
			return;
		}

		// Generate the report here.
		eval.report = event::conforms
		{
			event, opts.non_conform.report
		};

		// When opts.conforming is false a bad report is not an error.
		if(!opts.conforming)
			return;

		// Otherwise this will kill the eval
		if(!eval.report.clean())
			throw error
			{
				fault::INVALID, "Non-conforming event: %s", string(eval.report)
			};
	}
};

namespace ircd::m
{
	constexpr size_t event_conforms_num{num_of<event::conforms::code>()};
	extern const std::array<string_view, event_conforms_num> event_conforms_reflects;
}

decltype(ircd::m::event_conforms_reflects)
ircd::m::event_conforms_reflects
{
	"INVALID_OR_MISSING_EVENT_ID",
	"INVALID_OR_MISSING_ROOM_ID",
	"INVALID_OR_MISSING_SENDER_ID",
	"MISSING_TYPE",
	"MISSING_ORIGIN",
	"INVALID_ORIGIN",
	"INVALID_OR_MISSING_REDACTS_ID",
	"MISSING_CONTENT_MEMBERSHIP",
	"INVALID_CONTENT_MEMBERSHIP",
	"MISSING_MEMBER_STATE_KEY",
	"INVALID_MEMBER_STATE_KEY",
	"MISSING_PREV_EVENTS",
	"MISSING_AUTH_EVENTS",
	"DEPTH_NEGATIVE",
	"DEPTH_ZERO",
	"MISSING_SIGNATURES",
	"MISSING_ORIGIN_SIGNATURE",
	"MISMATCH_ORIGIN_SENDER",
	"MISMATCH_CREATE_SENDER",
	"MISMATCH_ALIASES_STATE_KEY",
	"SELF_REDACTS",
	"SELF_PREV_EVENT",
	"SELF_AUTH_EVENT",
	"DUP_PREV_EVENT",
	"DUP_AUTH_EVENT",
};

std::ostream &
ircd::m::operator<<(std::ostream &s, const event::conforms &conforms)
{
	thread_local char buf[1024];
	s << conforms.string(buf);
	return s;
}

ircd::string_view
ircd::m::reflect(const event::conforms::code &code)
try
{
	return event_conforms_reflects.at(code);
}
catch(const std::out_of_range &e)
{
	return "??????"_sv;
}

ircd::m::event::conforms::code
ircd::m::event::conforms::reflect(const string_view &name)
{
	const auto it
	{
		std::find(begin(event_conforms_reflects), end(event_conforms_reflects), name)
	};

	if(it == end(event_conforms_reflects))
		throw std::out_of_range
		{
			"There is no event::conforms code by that name."
		};

	return code(std::distance(begin(event_conforms_reflects), it));
}

ircd::m::event::conforms::conforms(const event &e,
                                   const uint64_t &skip)
:conforms{e}
{
	report &= ~skip;
}

ircd::m::event::conforms::conforms(const event &e)
:report{0}
{
	if(!e.event_id)
		set(INVALID_OR_MISSING_EVENT_ID);

	if(defined(json::get<"event_id"_>(e)))
		if(!valid(m::id::EVENT, json::get<"event_id"_>(e)))
			set(INVALID_OR_MISSING_EVENT_ID);

	if(!valid(m::id::ROOM, json::get<"room_id"_>(e)))
		set(INVALID_OR_MISSING_ROOM_ID);

	if(!valid(m::id::USER, json::get<"sender"_>(e)))
		set(INVALID_OR_MISSING_SENDER_ID);

	if(empty(json::get<"type"_>(e)))
		set(MISSING_TYPE);

	if(empty(json::get<"origin"_>(e)))
		set(MISSING_ORIGIN);

	//TODO: XXX
	if((false))
		set(INVALID_ORIGIN);

	if(empty(json::get<"signatures"_>(e)))
		set(MISSING_SIGNATURES);

	if(empty(json::object{json::get<"signatures"_>(e).get(json::get<"origin"_>(e))}))
		set(MISSING_ORIGIN_SIGNATURE);

	if(!has(INVALID_OR_MISSING_SENDER_ID))
		if(json::get<"origin"_>(e) != m::id::user{json::get<"sender"_>(e)}.host())
			set(MISMATCH_ORIGIN_SENDER);

	if(json::get<"type"_>(e) == "m.room.create")
		if(m::room::id(json::get<"room_id"_>(e)).host() != m::user::id(json::get<"sender"_>(e)).host())
			set(MISMATCH_CREATE_SENDER);

	if(json::get<"type"_>(e) == "m.room.aliases")
		if(m::user::id(json::get<"sender"_>(e)).host() != json::get<"state_key"_>(e))
			set(MISMATCH_ALIASES_STATE_KEY);

	if(json::get<"type"_>(e) == "m.room.redaction")
		if(!valid(m::id::EVENT, json::get<"redacts"_>(e)))
			set(INVALID_OR_MISSING_REDACTS_ID);

	if(json::get<"redacts"_>(e))
		if(json::get<"redacts"_>(e) == e.event_id)
			set(SELF_REDACTS);

	if(json::get<"type"_>(e) == "m.room.member")
		if(empty(unquote(json::get<"content"_>(e).get("membership"))))
			set(MISSING_CONTENT_MEMBERSHIP);

	if(json::get<"type"_>(e) == "m.room.member")
		if(!all_of<std::islower>(unquote(json::get<"content"_>(e).get("membership"))))
			set(INVALID_CONTENT_MEMBERSHIP);

	if(json::get<"type"_>(e) == "m.room.member")
		if(empty(json::get<"state_key"_>(e)))
			set(MISSING_MEMBER_STATE_KEY);

	if(json::get<"type"_>(e) == "m.room.member")
		if(!valid(m::id::USER, json::get<"state_key"_>(e)))
			set(INVALID_MEMBER_STATE_KEY);

	if(json::get<"type"_>(e) != "m.room.create")
		if(empty(json::get<"prev_events"_>(e)))
			set(MISSING_PREV_EVENTS);

	if(json::get<"type"_>(e) != "m.room.create")
		if(empty(json::get<"auth_events"_>(e)))
			set(MISSING_AUTH_EVENTS);

	if(json::get<"depth"_>(e) != json::undefined_number && json::get<"depth"_>(e) < 0)
		set(DEPTH_NEGATIVE);

	if(json::get<"type"_>(e) != "m.room.create")
		if(json::get<"depth"_>(e) == 0)
			set(DEPTH_ZERO);

	const event::prev prev{e};
	if(json::get<"event_id"_>(e))
	{
		size_t i{0};
		for(const json::array &pe : json::get<"prev_events"_>(prev))
		{
			if(unquote(pe.at(0)) == json::get<"event_id"_>(e))
				set(SELF_PREV_EVENT);

			++i;
		}

		i = 0;
		for(const json::array &ps : json::get<"auth_events"_>(prev))
		{
			if(unquote(ps.at(0)) == json::get<"event_id"_>(e))
				set(SELF_AUTH_EVENT);

			++i;
		}
	}

	for(size_t i(0); i < prev.auth_events_count(); ++i)
	{
		const auto &[event_id, ref_hash]
		{
			prev.auth_events(i)
		};

		for(size_t j(0); j < prev.auth_events_count(); ++j)
			if(i != j)
				if(event_id == prev.auth_event(j))
					set(DUP_AUTH_EVENT);
	}

	for(size_t i(0); i < prev.prev_events_count(); ++i)
	{
		const auto &[event_id, ref_hash]
		{
			prev.prev_events(i)
		};

		for(size_t j(0); j < prev.prev_events_count(); ++j)
			if(i != j)
				if(event_id == prev.prev_event(j))
					set(DUP_PREV_EVENT);
	}
}

void
ircd::m::event::conforms::operator|=(const code &code)
&
{
	set(code);
}

void
ircd::m::event::conforms::del(const code &code)
{
	report &= ~(1UL << code);
}

void
ircd::m::event::conforms::set(const code &code)
{
	report |= (1UL << code);
}

ircd::string_view
ircd::m::event::conforms::string(const mutable_buffer &out)
const
{
	mutable_buffer buf{out};
	for(uint64_t i(0); i < num_of<code>(); ++i)
	{
		if(!has(code(i)))
			continue;

		if(begin(buf) != begin(out))
			consume(buf, copy(buf, " "_sv));

		consume(buf, copy(buf, m::reflect(code(i))));
	}

	return { data(out), begin(buf) };
}

bool
ircd::m::event::conforms::has(const code &code)
const
{
	return report & (1UL << code);
}

bool
ircd::m::event::conforms::has(const uint &code)
const
{
	return (report & (1UL << code)) == code;
}

bool
ircd::m::event::conforms::operator!()
const
{
	return clean();
}

ircd::m::event::conforms::operator bool()
const
{
	return !clean();
}

bool
ircd::m::event::conforms::clean()
const
{
	return report == 0;
}

///////////////////////////////////////////////////////////////////////////////
//
// event/prefetch.h
//

void
ircd::m::prefetch(const event::id &event_id,
                  const event::fetch::opts &opts)
{
	prefetch(index(event_id), opts);
}

void
ircd::m::prefetch(const event::id &event_id,
                  const string_view &key)
{
	prefetch(index(event_id), key);
}

void
ircd::m::prefetch(const event::idx &event_idx,
                  const event::fetch::opts &opts)
{
	const event::keys keys{opts.keys};
	const vector_view<const string_view> cols{keys};
	for(const auto &col : cols)
		if(col)
			prefetch(event_idx, col);
}

void
ircd::m::prefetch(const event::idx &event_idx,
                  const string_view &key)
{
	const auto &column_idx
	{
		json::indexof<event>(key)
	};

	auto &column
	{
		dbs::event_column.at(column_idx)
	};

	db::prefetch(column, byte_view<string_view>{event_idx});
}

///////////////////////////////////////////////////////////////////////////////
//
// event/cached.h
//

bool
ircd::m::cached(const id::event &event_id)
{
	return cached(event_id, event::fetch::default_opts);
}

bool
ircd::m::cached(const id::event &event_id,
                const event::fetch::opts &opts)
{
	if(!db::cached(dbs::event_idx, event_id, opts.gopts))
		return false;

	const event::idx event_idx
	{
		index(event_id, std::nothrow)
	};

	return event_idx?
		cached(event_idx, opts.gopts):
		false;
}

bool
ircd::m::cached(const event::idx &event_idx)
{
	return cached(event_idx, event::fetch::default_opts);
}

bool
ircd::m::cached(const event::idx &event_idx,
                const event::fetch::opts &opts)
{
	const byte_view<string_view> &key
	{
		event_idx
	};

	if(event::fetch::should_seek_json(opts))
		return db::cached(dbs::event_json, key, opts.gopts);

	const auto &selection
	{
		opts.keys
	};

	const auto result
	{
		cached_keys(event_idx, opts)
	};

	for(size_t i(0); i < selection.size(); ++i)
	{
		auto &column
		{
			dbs::event_column.at(i)
		};

		if(column && selection.test(i) && !result.test(i))
		{
			if(!db::has(column, key, opts.gopts))
				continue;

			return false;
		}
	}

	return true;
}

ircd::m::event::keys::selection
ircd::m::cached_keys(const event::idx &event_idx,
                     const event::fetch::opts &opts)
{
	const byte_view<string_view> &key
	{
		event_idx
	};

	event::keys::selection ret(0);
	const event::keys::selection &selection(opts.keys);
	for(size_t i(0); i < selection.size(); ++i)
	{
		auto &column
		{
			dbs::event_column.at(i)
		};

		if(column && db::cached(column, key, opts.gopts))
			ret.set(i);
	}

	return ret;
}

///////////////////////////////////////////////////////////////////////////////
//
// event/get.h
//

std::string
ircd::m::get(const event::id &event_id,
             const string_view &key)
{
	std::string ret;
	get(event_id, key, [&ret]
	(const string_view &value)
	{
		ret = value;
	});

	return ret;
}

std::string
ircd::m::get(const event::idx &event_idx,
             const string_view &key)
{
	std::string ret;
	get(event_idx, key, [&ret]
	(const string_view &value)
	{
		ret = value;
	});

	return ret;
}

std::string
ircd::m::get(std::nothrow_t,
             const event::id &event_id,
             const string_view &key)
{
	std::string ret;
	get(std::nothrow, event_id, key, [&ret]
	(const string_view &value)
	{
		ret = value;
	});

	return ret;
}

std::string
ircd::m::get(std::nothrow_t,
             const event::idx &event_idx,
             const string_view &key)
{
	std::string ret;
	get(std::nothrow, event_idx, key, [&ret]
	(const string_view &value)
	{
		ret = value;
	});

	return ret;
}

ircd::const_buffer
ircd::m::get(const event::id &event_id,
             const string_view &key,
             const mutable_buffer &out)
{
	const auto &ret
	{
		get(std::nothrow, index(event_id), key, out)
	};

	if(!ret)
		throw m::NOT_FOUND
		{
			"%s for %s not found in database",
			key,
			string_view{event_id}
		};

	return ret;
}

ircd::const_buffer
ircd::m::get(const event::idx &event_idx,
             const string_view &key,
             const mutable_buffer &out)
{
	const const_buffer ret
	{
		get(std::nothrow, event_idx, key, out)
	};

	if(!ret)
		throw m::NOT_FOUND
		{
			"%s for event_idx[%lu] not found in database",
			key,
			event_idx
		};

	return ret;
}

ircd::const_buffer
ircd::m::get(std::nothrow_t,
             const event::id &event_id,
             const string_view &key,
             const mutable_buffer &buf)
{
	return get(std::nothrow, index(event_id), key, buf);
}

ircd::const_buffer
ircd::m::get(std::nothrow_t,
             const event::idx &event_idx,
             const string_view &key,
             const mutable_buffer &buf)
{
	const_buffer ret;
	get(std::nothrow, event_idx, key, [&buf, &ret]
	(const string_view &value)
	{
		ret = { data(buf), copy(buf, value) };
	});

	return ret;
}

void
ircd::m::get(const event::id &event_id,
             const string_view &key,
             const event::fetch::view_closure &closure)
{
	if(!get(std::nothrow, index(event_id), key, closure))
		throw m::NOT_FOUND
		{
			"%s for %s not found in database",
			key,
			string_view{event_id}
		};
}

void
ircd::m::get(const event::idx &event_idx,
             const string_view &key,
             const event::fetch::view_closure &closure)
{
	if(!get(std::nothrow, event_idx, key, closure))
		throw m::NOT_FOUND
		{
			"%s for event_idx[%lu] not found in database",
			key,
			event_idx
		};
}

bool
ircd::m::get(std::nothrow_t,
             const event::id &event_id,
             const string_view &key,
             const event::fetch::view_closure &closure)
{
	return get(std::nothrow, index(event_id), key, closure);
}

bool
ircd::m::get(std::nothrow_t,
             const event::idx &event_idx,
             const string_view &key,
             const event::fetch::view_closure &closure)
{
	const string_view &column_key
	{
		byte_view<string_view>{event_idx}
	};

	const auto &column_idx
	{
		json::indexof<event>(key)
	};

	auto &column
	{
		dbs::event_column.at(column_idx)
	};

	if(column)
		return column(column_key, std::nothrow, closure);

	// If the event property being sought doesn't have its own column we
	// fall back to fetching the full JSON and closing over the property.
	bool ret{false};
	dbs::event_json(column_key, std::nothrow, [&closure, &key, &ret]
	(const json::object &event)
	{
		string_view value
		{
			event[key]
		};

		if(!value)
			return;

		// The user expects an unquoted string to their closure, the same as
		// if this value was found in its own column.
		if(json::type(value) == json::STRING)
			value = unquote(value);

		ret = true;
		closure(value);
	});

	return ret;
}

///////////////////////////////////////////////////////////////////////////////
//
// event/fetch.h
//

//
// seek
//

void
ircd::m::seek(event::fetch &fetch,
              const event::id &event_id)
{
	if(!seek(fetch, event_id, std::nothrow))
		throw m::NOT_FOUND
		{
			"%s not found in database", event_id
		};
}

bool
ircd::m::seek(event::fetch &fetch,
              const event::id &event_id,
              std::nothrow_t)
{
	const auto &event_idx
	{
		index(event_id, std::nothrow)
	};

	if(!event_idx)
	{
		fetch.valid = false;
		return fetch.valid;
	}

	return seek(fetch, event_idx, event_id, std::nothrow);
}

void
ircd::m::seek(event::fetch &fetch,
              const event::idx &event_idx)
{
	if(!seek(fetch, event_idx, std::nothrow))
		throw m::NOT_FOUND
		{
			"%lu not found in database", event_idx
		};
}

bool
ircd::m::seek(event::fetch &fetch,
              const event::idx &event_idx,
              std::nothrow_t)
{
	const auto &event_id
	{
		m::event_id(event_idx, fetch.event_id_buf, std::nothrow)
	};

	return seek(fetch, event_idx, event_id, std::nothrow);
}

bool
ircd::m::seek(event::fetch &fetch,
              const event::idx &event_idx,
              const event::id &event_id,
              std::nothrow_t)
{
	fetch.event_idx = event_idx;
	fetch.event_id_buf = event_id;
	const string_view &key
	{
		byte_view<string_view>(event_idx)
	};

	auto &event
	{
		static_cast<m::event &>(fetch)
	};

	assert(fetch.fopts);
	const auto &opts(*fetch.fopts);
	if(!fetch.should_seek_json(opts))
		if((fetch.valid = db::seek(fetch.row, key, opts.gopts)))
			if((fetch.valid = fetch.assign_from_row(key)))
				return fetch.valid;

	if((fetch.valid = fetch._json.load(key, opts.gopts)))
		fetch.valid = fetch.assign_from_json(key);

	return fetch.valid;
}

//
// event::fetch
//

decltype(ircd::m::event::fetch::default_opts)
ircd::m::event::fetch::default_opts
{};

//
// event::fetch::fetch
//

/// Seek to event_id and populate this event from database.
/// Throws if event not in database.
ircd::m::event::fetch::fetch(const event::id &event_id,
                             const opts &opts)
:fetch
{
	index(event_id), event_id, std::nothrow, opts
}
{
	if(!valid)
		throw m::NOT_FOUND
		{
			"%s not found in database", string_view{event_id}
		};
}

/// Seek to event_id and populate this event from database.
/// Event is not populated if not found in database.
ircd::m::event::fetch::fetch(const event::id &event_id,
                             std::nothrow_t,
                             const opts &opts)
:fetch
{
	index(event_id, std::nothrow), event_id, std::nothrow, opts
}
{
}

/// Seek to event_idx and populate this event from database.
/// Throws if event not in database.
ircd::m::event::fetch::fetch(const event::idx &event_idx,
                             const opts &opts)
:fetch
{
	event_idx, std::nothrow, opts
}
{
	if(!valid)
		throw m::NOT_FOUND
		{
			"idx %zu not found in database", event_idx
		};
}

ircd::m::event::fetch::fetch(const event::idx &event_idx,
                             std::nothrow_t,
                             const opts &opts)
:fetch
{
	event_idx, m::event::id{}, std::nothrow, opts
}
{
}

/// Seek to event_idx and populate this event from database.
/// Event is not populated if not found in database.
ircd::m::event::fetch::fetch(const event::idx &event_idx,
                             const event::id &event_id,
                             std::nothrow_t,
                             const opts &opts)
:fopts
{
	&opts
}
,event_idx
{
	event_idx
}
,_json
{
	dbs::event_json,
	event_idx && should_seek_json(opts)?
		key(&event_idx):
		string_view{},
	opts.gopts
}
,row
{
	*dbs::events,
	event_idx && !_json.valid(key(&event_idx))?
		key(&event_idx):
		string_view{},
	event_idx && !_json.valid(key(&event_idx))?
		event::keys{opts.keys}:
		event::keys{event::keys::include{}},
	cell,
	opts.gopts
}
,valid
{
	false
}
,event_id_buf
{
	event_id?
		event::id::buf{event_id}:
		event::id::buf{}
}
{
	valid =
		event_idx && _json.valid(key(&event_idx))?
			assign_from_json(key(&event_idx)):
		event_idx?
			assign_from_row(key(&event_idx)):
			false;
}

/// Seekless constructor.
ircd::m::event::fetch::fetch(const opts &opts)
:fopts
{
	&opts
}
,_json
{
	dbs::event_json,
	string_view{},
	opts.gopts
}
,row
{
	*dbs::events,
	string_view{},
	!should_seek_json(opts)?
		event::keys{opts.keys}:
		event::keys{event::keys::include{}},
	cell,
	opts.gopts
}
,valid
{
	false
}
{
}

bool
ircd::m::event::fetch::assign_from_json(const string_view &key)
{
	auto &event
	{
		static_cast<m::event &>(*this)
	};

	assert(_json.valid(key));
	const json::object source
	{
		_json.val()
	};

	assert(!empty(source));
	const bool source_event_id
	{
		!event_id_buf || source.has("event_id")
	};

	const auto event_id
	{
		source_event_id?
			id(unquote(source.at("event_id"))):
		event_id_buf?
			id(event_id_buf):
			m::event_id(event_idx, event_id_buf, std::nothrow)
	};

	assert(fopts);
	assert(event_id);
	event =
	{
		source, event_id, event::keys{fopts->keys}
	};

	assert(data(event.source) == data(source));
	assert(event.event_id == event_id);
	return true;
}

bool
ircd::m::event::fetch::assign_from_row(const string_view &key)
{
	auto &event
	{
		static_cast<m::event &>(*this)
	};

	if(!row.valid(key))
		return false;

	event.source = {};
	assign(event, row, key);
	const auto event_id
	{
		defined(json::get<"event_id"_>(*this))?
			id{json::get<"event_id"_>(*this)}:
		event_id_buf?
			id{event_id_buf}:
			m::event_id(event_idx, event_id_buf, std::nothrow)
	};

	assert(event_id);
	event.event_id = event_id;
	return true;
}

bool
ircd::m::event::fetch::should_seek_json(const opts &opts)
{
	// User always wants to make the event_json query regardless
	// of their keys selection.
	if(opts.query_json_force)
		return true;

	// If and only if selected keys have direct columns we can return
	// false to seek direct columns. If any other keys are selected we
	/// must perform the event_json query instead.
	for(size_t i(0); i < opts.keys.size(); ++i)
		if(opts.keys.test(i))
			if(!dbs::event_column.at(i))
				return true;

	return false;
}

ircd::string_view
ircd::m::event::fetch::key(const event::idx *const &event_idx)
{
	assert(event_idx);
	return byte_view<string_view>(*event_idx);
}

//
// event::fetch::opts
//

ircd::m::event::fetch::opts::opts(const db::gopts &gopts,
                                  const event::keys::selection &keys)
:opts
{
	keys, gopts
}
{
}

ircd::m::event::fetch::opts::opts(const event::keys::selection &keys,
                                  const db::gopts &gopts)
:keys{keys}
,gopts{gopts}
{
}

///////////////////////////////////////////////////////////////////////////////
//
// event/event_id.h
//

ircd::m::event::id::buf
ircd::m::event_id(const event::idx &event_idx)
{
	event::id::buf ret;
	event_id(event_idx, ret);
	return ret;
}

ircd::m::event::id::buf
ircd::m::event_id(const event::idx &event_idx,
                  std::nothrow_t)
{
	event::id::buf ret;
	event_id(event_idx, ret, std::nothrow);
	return ret;
}

ircd::m::event::id
ircd::m::event_id(const event::idx &event_idx,
                  event::id::buf &buf)
{
	const event::id ret
	{
		event_id(event_idx, buf, std::nothrow)
	};

	if(!ret)
		throw m::NOT_FOUND
		{
			"Cannot find event ID from idx[%lu]", event_idx
		};

	return ret;
}

ircd::m::event::id
ircd::m::event_id(const event::idx &event_idx,
                  event::id::buf &buf,
                  std::nothrow_t)
{
	event_id(event_idx, std::nothrow, [&buf]
	(const event::id &eid)
	{
		buf = eid;
	});

	return buf? event::id{buf} : event::id{};
}

bool
ircd::m::event_id(const event::idx &event_idx,
                  std::nothrow_t,
                  const event::id::closure &closure)
{
	return get(std::nothrow, event_idx, "event_id", closure);
}

///////////////////////////////////////////////////////////////////////////////
//
// event/index.h
//

ircd::m::event::idx
ircd::m::index(const event &event)
try
{
	return index(event.event_id);
}
catch(const json::not_found &)
{
	throw m::NOT_FOUND
	{
		"Cannot find index for event without an event_id."
	};
}

ircd::m::event::idx
ircd::m::index(const event &event,
               std::nothrow_t)
try
{
	return index(event.event_id, std::nothrow);
}
catch(const json::not_found &)
{
	return 0;
}

ircd::m::event::idx
ircd::m::index(const event::id &event_id)
{
	const auto ret
	{
		index(event_id, std::nothrow)
	};

	if(!ret)
		throw m::NOT_FOUND
		{
			"no index found for %s",
			string_view{event_id}

		};

	return ret;
}

ircd::m::event::idx
ircd::m::index(const event::id &event_id,
               std::nothrow_t)
{
	event::idx ret{0};
	index(event_id, std::nothrow, [&ret]
	(const event::idx &event_idx)
	{
		ret = event_idx;
	});

	return ret;
}

bool
ircd::m::index(const event::id &event_id,
               std::nothrow_t,
               const event::closure_idx &closure)
{
	auto &column
	{
		dbs::event_idx
	};

	if(!event_id)
		return false;

	return column(event_id, std::nothrow, [&closure]
	(const string_view &value)
	{
		const event::idx &event_idx
		{
			byte_view<event::idx>(value)
		};

		closure(event_idx);
	});
}

///////////////////////////////////////////////////////////////////////////////
//
// event/auth.h
//

//
// event::auth
//

void
ircd::m::event::auth::check(const event &event)
{
	const auto reason
	{
		failed(event)
	};

	if(reason)
		throw m::ACCESS_DENIED
		{
			"Authorization of %s failed against its auth_events :%s",
			string_view{event.event_id},
			reason
		};
}

bool
ircd::m::event::auth::check(std::nothrow_t,
                            const event &event)
{
	return !failed(event);
}

ircd::string_view
ircd::m::event::auth::failed(const m::event &event)
{
	const m::event::prev refs(event);
	const auto count(refs.auth_events_count());
	if(count > 4)
		return "Too many auth_events references.";

	const m::event *authv[4];
	const m::event::fetch auth[4]
	{
		{ count > 0? refs.auth_event(0) : event::id{}, std::nothrow },
		{ count > 1? refs.auth_event(1) : event::id{}, std::nothrow },
		{ count > 2? refs.auth_event(2) : event::id{}, std::nothrow },
		{ count > 3? refs.auth_event(3) : event::id{}, std::nothrow },
	};

	size_t i(0), j(0);
	for(; i < 4; ++i)
		if(auth[i].valid)
			authv[j++] = &auth[i];

	return failed(event, {authv, j});
}

ircd::string_view
ircd::m::event::auth::failed(const m::event &event,
                             const vector_view<const m::event *> &auth_events)
{
	const m::event::prev refs{event};

	// 1. If type is m.room.create
	if(json::get<"type"_>(event) == "m.room.create")
	{
		// a. If it has any previous events, reject.
		if(count(refs) || !empty(auth_events))
			return "m.room.create has previous events.";

		// b. If the domain of the room_id does not match the domain of the
		// sender, reject.
		assert(!conforms(event).has(conforms::MISMATCH_CREATE_SENDER));
		if(conforms(event).has(conforms::MISMATCH_CREATE_SENDER))
			return "m.room.create room_id domain does not match sender domain.";

		// c. If content.room_version is present and is not a recognised
		// version, reject.
		if(json::get<"content"_>(event).has("room_version"))
			if(unquote(json::get<"content"_>(event).get("room_version")) != "1")
				return "m.room.create room_version is not recognized.";

		// d. If content has no creator field, reject.
		assert(!empty(json::get<"content"_>(event).get("creator")));
		if(empty(json::get<"content"_>(event).get("creator")))
			return "m.room.create content.creator is missing.";

		// e. Otherwise, allow.
		return {};
	}

	// 2. Reject if event has auth_events that:
	const m::event *auth_create{nullptr};
	const m::event *auth_power{nullptr};
	const m::event *auth_join_rules{nullptr};
	const m::event *auth_member_target{nullptr};
	const m::event *auth_member_sender{nullptr};
	for(size_t i(0); i < auth_events.size(); ++i)
	{
		// a. have duplicate entries for a given type and state_key pair
		assert(auth_events.at(i));
		const m::event &a(*auth_events.at(i));
		for(size_t j(0); j < auth_events.size(); ++j) if(i != j)
		{
			assert(auth_events.at(j));
			const auto &b(*auth_events.at(j));
			if(json::get<"type"_>(a) == json::get<"type"_>(b))
				if(json::get<"state_key"_>(a) == json::get<"state_key"_>(b))
					return "Duplicate (type,state_key) in auth_events.";
		}

		// b. have entries whose type and state_key don't match those specified by
		// the auth events selection algorithm described in the server...
		const auto &type(json::get<"type"_>(a));
		if(type == "m.room.create")
		{
			assert(!auth_create);
			auth_create = &a;
			continue;
		}

		if(type == "m.room.power_levels")
		{
			assert(!auth_power);
			auth_power = &a;
			continue;
		}

		if(type == "m.room.join_rules")
		{
			assert(!auth_join_rules);
			auth_join_rules = &a;
			continue;
		}

		if(type == "m.room.member")
		{
			if(json::get<"sender"_>(event) == json::get<"state_key"_>(a))
			{
				assert(!auth_member_sender);
				auth_member_sender = &a;
			}

			if(json::get<"state_key"_>(event) == json::get<"state_key"_>(a))
			{
				assert(!auth_member_target);
				auth_member_target = &a;
			}

			continue;
		}

		return "Reference in auth_events is not an auth_event.";
	}

	// 3. If event does not have a m.room.create in its auth_events, reject.
	if(!auth_create)
		return "Missing m.room.create in auth_events";

	const m::room::power power
	{
		auth_power? *auth_power : m::event{}, *auth_create
	};

	// 4. If type is m.room.aliases:
	if(json::get<"type"_>(event) == "m.room.aliases")
	{
		// a. If event has no state_key, reject.
		assert(!conforms(event).has(conforms::MISMATCH_ALIASES_STATE_KEY));
		if(empty(json::get<"state_key"_>(event)))
			return "m.room.aliases event is missing a state_key.";

		// b. If sender's domain doesn't matches state_key, reject.
		if(json::get<"state_key"_>(event) != m::user::id(json::get<"sender"_>(event)).host())
			return "m.room.aliases event state_key is not the sender's domain.";

		// c. Otherwise, allow
		return {};
	}

	// 5. If type is m.room.member:
	if(json::get<"type"_>(event) == "m.room.member")
	{
		// a. If no state_key key ...
		assert(!conforms(event).has(conforms::MISSING_MEMBER_STATE_KEY));
		if(empty(json::get<"state_key"_>(event)))
			return "m.room.member event is missing a state_key.";

		// a. ... or membership key in content, reject.
		assert(!conforms(event).has(conforms::MISSING_CONTENT_MEMBERSHIP));
		if(empty(unquote(json::get<"content"_>(event).get("membership"))))
			return "m.room.member event is missing a content.membership.";

		assert(!conforms(event).has(conforms::INVALID_MEMBER_STATE_KEY));
		if(!valid(m::id::USER, json::get<"state_key"_>(event)))
			return "m.room.member event state_key is not a valid user mxid.";

		// b. If membership is join
		if(membership(event) == "join")
		{
			// i. If the only previous event is an m.room.create and the
			// state_key is the creator, allow.
			if(refs.prev_events_count() == 1 && refs.auth_events_count() == 1)
				if(auth_create && auth_create->event_id == refs.prev_event(0))
					return {};

			// ii. If the sender does not match state_key, reject.
			if(json::get<"sender"_>(event) != json::get<"state_key"_>(event))
				return "m.room.member membership=join sender does not match state_key.";

			// iii. If the sender is banned, reject.
			if(auth_member_target)
				if(membership(*auth_member_target) == "ban")
					return "m.room.member membership=join references membership=ban auth_event.";

			if(auth_member_sender)
				if(membership(*auth_member_sender) == "ban")
					return "m.room.member membership=join references membership=ban auth_event.";

			if(auth_join_rules)
			{
				// iv. If the join_rule is invite then allow if membership state
				// is invite or join.
				if(unquote(json::get<"content"_>(*auth_join_rules).get("join_rule")) == "invite")
					if(auth_member_target)
					{
						if(membership(*auth_member_target) == "invite")
							return {};

						if(membership(*auth_member_target) == "join")
							return {};
					}

				// v. If the join_rule is public, allow.
				if(unquote(json::get<"content"_>(*auth_join_rules).get("join_rule")) == "public")
					return {};

			}

			// vi. Otherwise, reject.
			return "m.room.member membership=join fails authorization.";
		}

		// c. If membership is invite
		if(membership(event) == "invite")
		{
			// i. If content has third_party_invite key
			if(json::get<"content"_>(event).has("third_party_invite"))
			{
				//TODO: XXX
				// 1. If target user is banned, reject.
				// 2. If content.third_party_invite does not have a signed key, reject.
				// 3. If signed does not have mxid and token keys, reject.
				// 4. If mxid does not match state_key, reject.
				// 5. If there is no m.room.third_party_invite event in the current room state with state_key matching token, reject.
				// 6. If sender does not match sender of the m.room.third_party_invite, reject.
				// 7. If any signature in signed matches any public key in the m.room.third_party_invite event, allow. The public keys are in content of m.room.third_party_invite as:
				// 7.1. A single public key in the public_key field.
				// 7.2. A list of public keys in the public_keys field.
				// 8. Otherwise, reject.
				return "third_party_invite fails authorization.";
			}

			// ii. If the sender's current membership state is not join, reject.
			if(auth_member_sender)
				if(membership(*auth_member_sender) != "join")
					return "m.room.member membership=invite sender must have membership=join.";

			// iii. If target user's current membership state is join or ban, reject.
			if(auth_member_target)
			{
				if(membership(*auth_member_target) == "join")
					return "m.room.member membership=invite target cannot have membership=join.";

				if(membership(*auth_member_target) == "ban")
					return "m.room.member membership=invite target cannot have membership=ban.";
			}

			// iv. If the sender's power level is greater than or equal to the invite level,
			// allow.
			if(power(at<"sender"_>(event), "invite"))
				return {};

			// v. Otherwise, reject.
			return "m.room.member membership=invite fails authorization.";
		}

		// d. If membership is leave
		if(membership(event) == "leave")
		{
			// i. If the sender matches state_key, allow if and only if that
			// user's current membership state is invite or join.
			if(json::get<"sender"_>(event) == json::get<"state_key"_>(event))
			{
				if(auth_member_target && membership(*auth_member_target) == "join")
					return {};

				if(auth_member_target && membership(*auth_member_target) == "invite")
					return {};

				return "m.room.member membership=leave self-target must have membership=join|invite.";
			}

			// ii. If the sender's current membership state is not join, reject.
			if(auth_member_sender)
				if(membership(*auth_member_sender) != "join")
					return "m.room.member membership=leave sender must have membership=join.";

			// iii. If the target user's current membership state is ban, and the sender's
			// power level is less than the ban level, reject.
			if(auth_member_target)
				if(membership(*auth_member_target) == "ban")
					if(!power(at<"sender"_>(event), "ban"))
						return "m.room.member membership=ban->leave sender must have ban power to unban.";

			// iv. If the sender's power level is greater than or equal to the
			// kick level, and the target user's power level is less than the
			// sender's power level, allow.
			if(power(at<"sender"_>(event), "kick"))
				if(power.level_user(at<"state_key"_>(event)) < power.level_user(at<"sender"_>(event)))
					return {};

			// v. Otherwise, reject.
			return "m.room.member membership=leave fails authorization.";
		}

		// e. If membership is ban
		if(membership(event) == "ban")
		{
			// i. If the sender's current membership state is not join, reject.
			if(auth_member_sender)
				if(membership(*auth_member_sender) != "join")
					return "m.room.member membership=ban sender must have membership=join.";

			// ii. If the sender's power level is greater than or equal to the
			// ban level, and the target user's power level is less than the
			// sender's power level, allow.
			if(power(at<"sender"_>(event), "ban"))
				if(power.level_user(at<"state_key"_>(event)) < power.level_user(at<"sender"_>(event)))
					return {};

			// iii. Otherwise, reject.
			return "m.room.member membership=ban fails authorization.";
		}

		// f. Otherwise, the membership is unknown. Reject.
		return "m.room.member membership=unknown.";
	}

	// 6. If the sender's current membership state is not join, reject.
	if(auth_member_sender)
		if(membership(*auth_member_sender) != "join")
			return "sender is not joined to room.";

	// 7. If type is m.room.third_party_invite:
	if(json::get<"type"_>(event) == "m.room.third_party_invite")
	{
		// a. Allow if and only if sender's current power level is greater
		// than or equal to the invite level.
		if(power(at<"sender"_>(event), "invite"))
			return {};

		return "sender has power level less than required for invite.";
	}

	// 8. If the event type's required power level is greater than the
	// sender's power level, reject.
	if(!power(at<"sender"_>(event), "events", at<"type"_>(event)))
		return "sender has insufficient power for event type.";

	// 9. If the event has a state_key that starts with an @ and does not
	// match the sender, reject.
	if(startswith(json::get<"state_key"_>(event), '@'))
		if(at<"state_key"_>(event) != at<"sender"_>(event))
			return "sender cannot set another user's mxid in a state_key.";

	// 10. If type is m.room.power_levels:
	if(json::get<"type"_>(event) == "m.room.power_levels")
	{
		// a. If users key in content is not a dictionary with keys that are
		// valid user IDs with values that are integers (or a string that is
		// an integer), reject.
		if(json::type(json::get<"content"_>(event).get("users")) != json::OBJECT)
			return "m.room.power_levels content.users is not a json object.";

		for(const auto &member : json::object(at<"content"_>(event).at("users")))
		{
			if(!m::valid(m::id::USER, member.first))
				return "m.room.power_levels content.users key is not a user mxid";

			if(!try_lex_cast<int64_t>(unquote(member.second)))
				return "m.room.power_levels content.users value is not an integer.";
		}

		// b. If there is no previous m.room.power_levels event in the room, allow.
		if(!auth_power)
			return {};

		const m::room::power old_power{*auth_power, *auth_create};
		const m::room::power new_power{event, *auth_create};
		const int64_t current_level
		{
			old_power.level_user(at<"sender"_>(event))
		};

		// c. For each of the keys users_default, events_default,
		// state_default, ban, redact, kick, invite, as well as each entry
		// being changed under the events or users keys:
		static const string_view keys[]
		{
			"users_default",
			"events_default",
			"state_default",
			"ban",
			"redact",
			"kick",
			"invite",
		};

		for(const auto &key : keys)
		{
			const int64_t old_level(old_power.level(key));
			const int64_t new_level(new_power.level(key));
			if(old_level == new_level)
				continue;

			// i. If the current value is higher than the sender's current
			// power level, reject.
			if(old_level > current_level)
				return "m.room.power_levels property denied to sender's power level.";

			// ii. If the new value is higher than the sender's current power
			// level, reject.
			if(new_level > current_level)
				return "m.room.power_levels property exceeds sender's power level.";
		}

		string_view ret;
		using closure = m::room::power::closure_bool;
		old_power.for_each("users", closure{[&ret, &new_power, &current_level]
		(const string_view &user_id, const int64_t &old_level)
		{
			if(new_power.has_user(user_id))
				if(new_power.level_user(user_id) == old_level)
					return true;

			// i. If the current value is higher than the sender's current
			// power level, reject.
			if(old_level > current_level)
			{
				ret = "m.room.power_levels user property denied to sender's power level.";
				return false;
			};

			// ii. If the new value is higher than the sender's current power
			// level, reject.
			if(new_power.level_user(user_id) > current_level)
			{
				ret = "m.room.power_levels user property exceeds sender's power level.";
				return false;
			};

			return true;
		}});

		if(ret)
			return ret;

		new_power.for_each("users", closure{[&ret, &old_power, &current_level]
		(const string_view &user_id, const int64_t &new_level)
		{
			if(old_power.has_user(user_id))
				if(old_power.level_user(user_id) == new_level)
					return true;

			// i. If the current value is higher than the sender's current
			// power level, reject.
			if(new_level > current_level)
			{
				ret = "m.room.power_levels user property exceeds sender's power level.";
				return false;
			};

			return true;
		}});

		if(ret)
			return ret;

		old_power.for_each("events", closure{[&ret, &new_power, &current_level]
		(const string_view &type, const int64_t &old_level)
		{
			if(new_power.has_event(type))
				if(new_power.level_event(type) == old_level)
					return true;

			// i. If the current value is higher than the sender's current
			// power level, reject.
			if(old_level > current_level)
			{
				ret = "m.room.power_levels event property denied to sender's power level.";
				return false;
			};

			// ii. If the new value is higher than the sender's current power
			// level, reject.
			if(new_power.level_event(type) > current_level)
			{
				ret = "m.room.power_levels event property exceeds sender's power level.";
				return false;
			};

			return true;
		}});

		if(ret)
			return ret;

		new_power.for_each("events", closure{[&ret, &old_power, &current_level]
		(const string_view &type, const int64_t &new_level)
		{
			if(old_power.has_event(type))
				if(old_power.level_event(type) == new_level)
					return true;

			// i. If the current value is higher than the sender's current
			// power level, reject.
			if(new_level > current_level)
			{
				ret = "m.room.power_levels event property exceeds sender's power level.";
				return false;
			};

			return true;
		}});

		// d. For each entry being changed under the users key...
		old_power.for_each("users", closure{[&ret, &event, &new_power, &current_level]
		(const string_view &user_id, const int64_t &old_level)
		{
			// ...other than the sender's own entry:
			if(user_id == at<"sender"_>(event))
				return true;

			if(new_power.has_user(user_id))
				if(new_power.level_user(user_id) == old_level)
					return true;

			// i. If the current value is equal to the sender's current power
			// level, reject.
			if(old_level == current_level)
			{
				ret = "m.room.power_levels user property denied to sender's power level.";
				return false;
			};

			return true;
		}});

		if(ret)
			return ret;

		// e. Otherwise, allow.
		assert(!ret);
		return ret;
	}

	// 11. If type is m.room.redaction:
	if(json::get<"type"_>(event) == "m.room.redaction")
	{
		// a. If the sender's power level is greater than or equal to the
		// redact level, allow.
		if(power(at<"sender"_>(event), "redact"))
			return {};

		// b. If the domain of the event_id of the event being redacted is the
		// same as the domain of the event_id of the m.room.redaction, allow.
		if(event::id(json::get<"redacts"_>(event)).host() == user::id(at<"sender"_>(event)).host())
			return {};

		// c. Otherwise, reject.
		return "m.room.redaction fails authorization.";
	}

	// 12. Otherwise, allow.
	return {};
}

bool
ircd::m::event::auth::is_power_event(const m::event &event)
{
	if(!json::get<"type"_>(event))
		return false;

	if(json::get<"type"_>(event) == "m.room.create")
		return true;

	if(json::get<"type"_>(event) == "m.room.power_levels")
		return true;

	if(json::get<"type"_>(event) == "m.room.join_rules")
		return true;

	if(json::get<"type"_>(event) != "m.room.member")
		return false;

	if(!json::get<"sender"_>(event) || !json::get<"state_key"_>(event))
		return false;

	if(json::get<"sender"_>(event) == json::get<"state_key"_>(event))
		return false;

	if(membership(event) == "leave" || membership(event) == "ban")
		return true;

	return false;
}

//
// event::auth::refs
//

size_t
ircd::m::event::auth::refs::count()
const noexcept
{
	return count(string_view{});
}

size_t
ircd::m::event::auth::refs::count(const string_view &type)
const noexcept
{
	size_t ret(0);
	for_each(type, [&ret](const auto &)
	{
		++ret;
		return true;
	});

	return ret;
}

bool
ircd::m::event::auth::refs::has(const event::idx &idx)
const noexcept
{
	return !for_each([&idx](const event::idx &ref)
	{
		return ref != idx; // true to continue, false to break
	});
}

bool
ircd::m::event::auth::refs::has(const string_view &type)
const noexcept
{
	bool ret{false};
	for_each(type, [&ret](const auto &)
	{
		ret = true;
		return false;
	});

	return ret;
}

bool
ircd::m::event::auth::refs::for_each(const closure_bool &closure)
const
{
	return for_each(string_view{}, closure);
}

bool
ircd::m::event::auth::refs::for_each(const string_view &type,
                                     const closure_bool &closure)
const
{
	assert(idx);
	const event::refs erefs
	{
		idx
	};

	return erefs.for_each(dbs::ref::NEXT_AUTH, [this, &type, &closure]
	(const event::idx &ref, const dbs::ref &)
	{
		bool match;
		const auto matcher
		{
			[&type, &match](const string_view &type_)
			{
				match = type == type_;
			}
		};

		if(type)
		{
			if(!m::get(std::nothrow, ref, "type", matcher))
				return true;

			if(!match)
				return true;
		}

		assert(idx != ref);
		if(!closure(ref))
			return false;

		return true;
	});
}

//
// event::auth::chain
//

size_t
ircd::m::event::auth::chain::depth()
const noexcept
{
	size_t ret(0);
	for_each([&ret](const auto &)
	{
		++ret;
	});

	return ret;
}

bool
ircd::m::event::auth::chain::has(const string_view &type)
const noexcept
{
	bool ret(false);
	for_each(closure_bool{[&type, &ret]
	(const auto &idx)
	{
		m::get(std::nothrow, idx, "type", [&type, &ret]
		(const auto &value)
		{
			ret = value == type;
		});

		return !ret;
	}});

	return ret;
}

bool
ircd::m::event::auth::chain::for_each(const closure &closure)
const
{
	return for_each(closure_bool{[&closure]
	(const auto &idx)
	{
		closure(idx);
		return true;
	}});
}

bool
ircd::m::event::auth::chain::for_each(const closure_bool &closure)
const
{
	return chain::for_each(*this, closure);
}

bool
ircd::m::event::auth::chain::for_each(const auth::chain &c,
                                      const closure_bool &closure)
{
	m::event::fetch e, a;
	std::set<event::idx> ae;
	std::deque<event::idx> aq {c.idx}; do
	{
		const auto idx(aq.front());
		aq.pop_front();
		if(!seek(e, idx, std::nothrow))
			continue;

		const m::event::prev prev{e};
		const auto count(prev.auth_events_count());
		for(size_t i(0); i < count && i < 4; ++i)
		{
			const auto &auth_event_id(prev.auth_event(i));
			const auto &auth_event_idx(index(auth_event_id, std::nothrow));
			if(!auth_event_idx)
				continue;

			auto it(ae.lower_bound(auth_event_idx));
			if(it == end(ae) || *it != auth_event_idx)
			{
				seek(a, auth_event_idx, std::nothrow);
				ae.emplace_hint(it, auth_event_idx);
				if(a.valid)
					aq.emplace_back(auth_event_idx);
			}
		}
	}
	while(!aq.empty());

	for(const auto &idx : ae)
		if(!closure(idx))
			return false;

	return true;
}

///////////////////////////////////////////////////////////////////////////////
//
// event/horizon.h
//

size_t
ircd::m::event::horizon::rebuild()
{
	throw not_implemented{};
}

bool
ircd::m::event::horizon::has(const event::id &event_id)
{
	char buf[m::dbs::EVENT_HORIZON_KEY_MAX_SIZE];
	const string_view &key
	{
		m::dbs::event_horizon_key(buf, event_id, 0UL)
	};

	auto it
	{
		m::dbs::event_horizon.begin(key)
	};

	return bool(it);
}

size_t
ircd::m::event::horizon::count()
const
{
	size_t ret(0);
	for_each([&ret]
	(const auto &, const auto &)
	{
		++ret;
		return true;
	});

	return ret;
}

bool
ircd::m::event::horizon::has(const event::idx &event_idx)
const
{
	return !for_each([&event_idx]
	(const auto &, const auto &_event_idx)
	{
		// false to break; true to continue.
		return _event_idx == event_idx? false : true;
	});
}

bool
ircd::m::event::horizon::for_each(const closure_bool &closure)
const
{
	if(!this->event_id)
		return for_every(closure);

	char buf[m::dbs::EVENT_HORIZON_KEY_MAX_SIZE];
	const string_view &key
	{
		m::dbs::event_horizon_key(buf, event_id, 0UL)
	};

	for(auto it(m::dbs::event_horizon.begin(key)); it; ++it)
	{
		const auto &event_idx
		{
			std::get<0>(m::dbs::event_horizon_key(it->first))
		};

		if(!closure(event_id, event_idx))
			return false;
	}

	return true;
}

bool
ircd::m::event::horizon::for_every(const closure_bool &closure)
{
	db::column &column{dbs::event_horizon};
	for(auto it(column.begin()); it; ++it)
	{
		const auto &parts
		{
			split(it->first, "\0"_sv)
		};

		const auto &event_id
		{
			parts.first
		};

		const auto &event_idx
		{
			byte_view<event::idx>(parts.second)
		};

		if(!closure(event_id, event_idx))
			return false;
	}

	return true;
}

///////////////////////////////////////////////////////////////////////////////
//
// event/refs.h
//

void
ircd::m::event::refs::rebuild()
{
	static const size_t pool_size{96};
	static const size_t log_interval{8192};

	db::txn txn
	{
		*m::dbs::events
	};

	auto &column
	{
		dbs::event_json
	};

	auto it
	{
		column.begin()
	};

	ctx::dock dock;
	ctx::pool pool;
	pool.min(pool_size);

	size_t i(0), j(0);
	const ctx::uninterruptible::nothrow ui;
	for(; it; ++it)
	{
		if(ctx::interruption_requested())
			break;

		const m::event::idx event_idx
		{
			byte_view<m::event::idx>(it->first)
		};

		std::string event{it->second};
		pool([&txn, &dock, &i, &j, event(std::move(event)), event_idx]
		{
			m::dbs::write_opts wopts;
			wopts.event_idx = event_idx;
			wopts.appendix.reset();
			wopts.appendix.set(dbs::appendix::EVENT_REFS);
			m::dbs::write(txn, json::object{event}, wopts);

			if(++j % log_interval == 0) log::info
			{
				m::log, "Refs builder @%zu:%zu of %lu (@idx: %lu)",
				i,
				j,
				m::vm::sequence::retired,
				event_idx
			};

			if(j >= i)
				dock.notify_one();
		});

		++i;
	}

	dock.wait([&i, &j]
	{
		return i == j;
	});

	txn();
}

size_t
ircd::m::event::refs::count()
const noexcept
{
	return count(dbs::ref(-1));
}

size_t
ircd::m::event::refs::count(const dbs::ref &type)
const noexcept
{
	assert(idx);
	size_t ret(0);
	for_each(type, [&ret]
	(const event::idx &ref, const dbs::ref &)
	{
		++ret;
		return true;
	});

	return ret;
}

bool
ircd::m::event::refs::has(const event::idx &idx)
const noexcept
{
	return has(dbs::ref(-1), idx);
}

bool
ircd::m::event::refs::has(const dbs::ref &type,
                          const event::idx &idx)
const noexcept
{
	return !for_each(type, [&idx]
	(const event::idx &ref, const dbs::ref &)
	{
		return ref != idx; // true to continue, false to break
	});
}

bool
ircd::m::event::refs::for_each(const closure_bool &closure)
const
{
	return for_each(dbs::ref(-1), closure);
}

bool
ircd::m::event::refs::for_each(const dbs::ref &type,
                               const closure_bool &closure)
const
{
	assert(idx);
	thread_local char buf[dbs::EVENT_REFS_KEY_MAX_SIZE];

	// Allow -1 to iterate through all types by starting
	// the iteration at type value 0 and then ignoring the
	// type as a loop continue condition.
	const bool all_type(type == dbs::ref(uint8_t(-1)));
	const auto &_type{all_type? dbs::ref::NEXT : type};
	assert(uint8_t(dbs::ref::NEXT) == 0);
	const string_view key
	{
		dbs::event_refs_key(buf, idx, _type, 0)
	};

	auto it
	{
		dbs::event_refs.begin(key)
	};

	for(; it; ++it)
	{
		const auto parts
		{
			dbs::event_refs_key(it->first)
		};

		const auto &type
		{
			std::get<0>(parts)
		};

		if(!all_type && type != _type)
			break;

		const auto &ref
		{
			std::get<1>(parts)
		};

		assert(idx != ref);
		if(!closure(ref, type))
			return false;
	}

	return true;
}

///////////////////////////////////////////////////////////////////////////////
//
// event/prev.h
//

bool
ircd::m::event::prev::prev_event_exists(const size_t &idx)
const
{
	return m::exists(prev_event(idx));
}

bool
ircd::m::event::prev::auth_event_exists(const size_t &idx)
const
{
	return m::exists(auth_event(idx));
}

bool
ircd::m::event::prev::prev_events_has(const event::id &event_id)
const
{
	for(size_t i(0); i < prev_events_count(); ++i)
		if(prev_event(i) == event_id)
			return true;

	return false;
}

bool
ircd::m::event::prev::auth_events_has(const event::id &event_id)
const
{
	for(size_t i(0); i < auth_events_count(); ++i)
		if(auth_event(i) == event_id)
			return true;

	return false;
}

size_t
ircd::m::event::prev::prev_events_count()
const
{
	return json::get<"prev_events"_>(*this).count();
}

size_t
ircd::m::event::prev::auth_events_count()
const
{
	return json::get<"auth_events"_>(*this).count();
}

ircd::m::event::id
ircd::m::event::prev::auth_event(const size_t &idx)
const
{
	return std::get<0>(auth_events(idx));
}

ircd::m::event::id
ircd::m::event::prev::prev_event(const size_t &idx)
const
{
	return std::get<0>(prev_events(idx));
}

std::tuple<ircd::m::event::id, ircd::json::object>
ircd::m::event::prev::auth_events(const size_t &idx)
const
{
	const string_view &prev_
	{
		at<"auth_events"_>(*this).at(idx)
	};

	switch(json::type(prev_))
	{
		// v1 event format
		case json::ARRAY:
		{
			const json::array &prev(prev_);
			const json::string &prev_id(prev.at(0));
			return {prev_id, prev[1]};
		}

		// v3/v4 event format
		case json::STRING:
		{
			const json::string &prev_id(prev_);
			return {prev_id, string_view{}};
		}

		default: throw m::INVALID_MXID
		{
			"auth_events[%zu] is invalid", idx
		};
	}
}

std::tuple<ircd::m::event::id, ircd::json::object>
ircd::m::event::prev::prev_events(const size_t &idx)
const
{
	const string_view &prev_
	{
		at<"prev_events"_>(*this).at(idx)
	};

	switch(json::type(prev_))
	{
		// v1 event format
		case json::ARRAY:
		{
			const json::array &prev(prev_);
			const json::string &prev_id(prev.at(0));
			return {prev_id, prev[1]};
		}

		// v3/v4 event format
		case json::STRING:
		{
			const json::string &prev_id(prev_);
			return {prev_id, string_view{}};
		}

		default: throw m::INVALID_MXID
		{
			"prev_events[%zu] is invalid", idx
		};
	}
}

void
ircd::m::for_each(const event::prev &prev,
                  const event::id::closure &closure)
{
	m::for_each(prev, event::id::closure_bool{[&closure]
	(const event::id &event_id)
	{
		closure(event_id);
		return true;
	}});
}

bool
ircd::m::for_each(const event::prev &prev,
                  const event::id::closure_bool &closure)
{
	return json::until(prev, [&closure]
	(const auto &key, const json::array &prevs)
	{
		for(const string_view &prev_ : prevs)
		{
			switch(json::type(prev_))
			{
				// v1 event format
				case json::ARRAY:
				{
					const json::array &prev(prev_);
					const json::string &prev_id(prev.at(0));
					if(!closure(prev_id))
						return false;

					continue;
				}

				// v3/v4 event format
				case json::STRING:
				{
					const json::string &prev(prev_);
					if(!closure(prev))
						return false;

					continue;
				}

				default:
					continue;
			}
		}

		return true;
	});
}

///////////////////////////////////////////////////////////////////////////////
//
// event/event.h
//

namespace ircd::m
{
	static json::object make_hashes(const mutable_buffer &out, const sha256::buf &hash);
}

/// The maximum size of an event we will create. This may also be used in
/// some contexts for what we will accept, but the protocol limit and hard
/// worst-case buffer size is still event::MAX_SIZE.
decltype(ircd::m::event::max_size)
ircd::m::event::max_size
{
	{ "name",     "m.event.max_size" },
	{ "default",   65507L            },
};

ircd::json::object
ircd::m::hashes(const mutable_buffer &out,
                const event &event)
{
	const sha256::buf hash_
	{
		hash(event)
	};

	return make_hashes(out, hash_);
}

ircd::json::object
ircd::m::event::hashes(const mutable_buffer &out,
                       json::iov &event,
                       const string_view &content)
{
	const sha256::buf hash_
	{
		hash(event, content)
	};

	return make_hashes(out, hash_);
}

ircd::json::object
ircd::m::make_hashes(const mutable_buffer &out,
                     const sha256::buf &hash)
{
	static const auto b64bufsz(b64encode_size(sizeof(hash)));
	thread_local char hashb64buf[b64bufsz];
	const json::members hashes
	{
		{ "sha256", b64encode_unpadded(hashb64buf, hash) }
	};

	return json::stringify(mutable_buffer{out}, hashes);
}

ircd::sha256::buf
ircd::m::event::hash(const json::object &event)
try
{
	static const size_t iov_max{json::iov::max_size};
	thread_local std::array<json::object::member, iov_max> member;

	size_t i(0);
	for(const auto &m : event)
	{
		if(m.first == "signatures" ||
		   m.first == "hashes" ||
		   m.first == "unsigned" ||
		   m.first == "age_ts" ||
		   m.first == "outlier" ||
		   m.first == "destinations")
			continue;

		member.at(i++) = m;
	}

	thread_local char buf[event::MAX_SIZE];
	const string_view reimage
	{
		json::stringify(buf, member.data(), member.data() + i)
	};

	return sha256{reimage};
}
catch(const std::out_of_range &e)
{
	throw m::BAD_JSON
	{
		"Object has more than %zu member properties.",
		json::iov::max_size
	};
}

ircd::sha256::buf
ircd::m::event::hash(json::iov &event,
                     const string_view &content)
{
	const json::iov::push _content
	{
		event, { "content", content }
	};

	return m::hash(m::event{event});
}

ircd::sha256::buf
ircd::m::hash(const event &event)
{
	if(event.source)
		return event::hash(event.source);

	m::event event_{event};
	json::get<"signatures"_>(event_) = {};
	json::get<"hashes"_>(event_) = {};

	thread_local char buf[event::MAX_SIZE];
	const string_view preimage
	{
		stringify(buf, event_)
	};

	return sha256{preimage};
}

bool
ircd::m::verify_hash(const event &event)
{
	const sha256::buf hash
	{
		m::hash(event)
	};

	return verify_hash(event, hash);
}

bool
ircd::m::verify_hash(const event &event,
                     const sha256::buf &hash)
{
	static constexpr size_t hashb64sz
	{
		size_t(sha256::digest_size * 1.34) + 1
	};

	thread_local char b64buf[hashb64sz];
	return verify_sha256b64(event, b64encode_unpadded(b64buf, hash));
}

bool
ircd::m::verify_sha256b64(const event &event,
                          const string_view &b64)
try
{
	const json::object &object
	{
		at<"hashes"_>(event)
	};

	const string_view &hash
	{
		unquote(object.at("sha256"))
	};

	return hash == b64;
}
catch(const json::not_found &)
{
	return false;
}

ircd::json::object
ircd::m::event::signatures(const mutable_buffer &out,
                           json::iov &event,
                           const json::iov &content)
{
	const ed25519::sig sig
	{
		sign(event, content)
	};

	thread_local char sigb64buf[b64encode_size(sizeof(sig))];
	const json::members sigb64
	{
		{ self::public_key_id, b64encode_unpadded(sigb64buf, sig) }
	};

	const json::members sigs
	{
		{ event.at("origin"), sigb64 }
    };

	return json::stringify(mutable_buffer{out}, sigs);
}

ircd::m::event
ircd::m::signatures(const mutable_buffer &out_,
                    const m::event &event_)
{
	thread_local char content[event::MAX_SIZE];
	m::event event
	{
		essential(event_, content)
	};

	thread_local char buf[event::MAX_SIZE];
	const json::object &preimage
	{
		stringify(buf, event)
	};

	const ed25519::sig sig
	{
		sign(preimage)
	};

	const auto sig_host
	{
		my_host(json::get<"origin"_>(event))?
			json::get<"origin"_>(event):
			my_host()
	};

	thread_local char sigb64buf[b64encode_size(sizeof(sig))];
	const json::member my_sig
	{
		sig_host, json::members
		{
			{ self::public_key_id, b64encode_unpadded(sigb64buf, sig) }
		}
	};

	static const size_t SIG_MAX{64};
	thread_local std::array<json::member, SIG_MAX> sigs;

	size_t i(0);
	sigs.at(i++) = my_sig;
	for(const auto &other : json::get<"signatures"_>(event_))
		if(!my_host(unquote(other.first)))
			sigs.at(i++) = { other.first, other.second };

	event = event_;
	mutable_buffer out{out_};
	json::get<"signatures"_>(event) = json::stringify(out, sigs.data(), sigs.data() + i);
	return event;
}

ircd::ed25519::sig
ircd::m::event::sign(json::iov &event,
                     const json::iov &contents)
{
	return sign(event, contents, self::secret_key);
}

ircd::ed25519::sig
ircd::m::event::sign(json::iov &event,
                     const json::iov &contents,
                     const ed25519::sk &sk)
{
	ed25519::sig sig;
	essential(event, contents, [&sk, &sig]
	(json::iov &event)
	{
		sig = m::sign(m::event{event}, sk);
	});

	return sig;
}

ircd::ed25519::sig
ircd::m::sign(const event &event)
{
	return sign(event, self::secret_key);
}

ircd::ed25519::sig
ircd::m::sign(const event &event,
              const ed25519::sk &sk)
{
	thread_local char buf[event::MAX_SIZE];
	const string_view preimage
	{
		stringify(buf, event)
	};

	return event::sign(preimage, sk);
}

ircd::ed25519::sig
ircd::m::event::sign(const json::object &event)
{
	return sign(event, self::secret_key);
}

ircd::ed25519::sig
ircd::m::event::sign(const json::object &event,
                     const ed25519::sk &sk)
{
	//TODO: skip rewrite
	thread_local char buf[event::MAX_SIZE];
	const string_view preimage
	{
		stringify(buf, event)
	};

	return sign(preimage, sk);
}

ircd::ed25519::sig
ircd::m::event::sign(const string_view &event)
{
	return sign(event, self::secret_key);
}

ircd::ed25519::sig
ircd::m::event::sign(const string_view &event,
                     const ed25519::sk &sk)
{
	const ed25519::sig sig
	{
		sk.sign(event)
	};

	return sig;
}
bool
ircd::m::verify(const event &event)
{
	const string_view &origin
	{
		at<"origin"_>(event)
	};

	return verify(event, origin);
}

bool
ircd::m::verify(const event &event,
                const string_view &origin)
{
	const json::object &signatures
	{
		at<"signatures"_>(event)
	};

	const json::object &origin_sigs
	{
		signatures.at(origin)
	};

	for(const auto &p : origin_sigs)
		if(verify(event, origin, unquote(p.first)))
			return true;

	return false;
}

bool
ircd::m::verify(const event &event,
                const string_view &origin,
                const string_view &keyid)
try
{
	const m::node node
	{
		origin
	};

	bool ret{false};
	node.key(keyid, [&ret, &event, &origin, &keyid]
	(const ed25519::pk &pk)
	{
		ret = verify(event, pk, origin, keyid);
	});

	return ret;
}
catch(const m::NOT_FOUND &e)
{
	log::derror
	{
		"Failed to verify %s because key %s for %s :%s",
		string_view{event.event_id},
		keyid,
		origin,
		e.what()
	};

	return false;
}

bool
ircd::m::verify(const event &event,
                const ed25519::pk &pk,
                const string_view &origin,
                const string_view &keyid)
{
	const json::object &signatures
	{
		at<"signatures"_>(event)
	};

	const json::object &origin_sigs
	{
		signatures.at(origin)
	};

	const ed25519::sig sig
	{
		[&origin_sigs, &keyid](auto &buf)
		{
			b64decode(buf, unquote(origin_sigs.at(keyid)));
		}
	};

	return verify(event, pk, sig);
}

bool
ircd::m::verify(const event &event_,
                const ed25519::pk &pk,
                const ed25519::sig &sig)
{
	thread_local char buf[2][event::MAX_SIZE];
	m::event event
	{
		essential(event_, buf[0])
	};

	const json::object &preimage
	{
		stringify(buf[1], event)
	};

	return pk.verify(preimage, sig);
}

bool
ircd::m::event::verify(const json::object &event,
                       const ed25519::pk &pk,
                       const ed25519::sig &sig)
{
	thread_local char buf[event::MAX_SIZE];
	const string_view preimage
	{
		stringify(buf, event)
	};

	return pk.verify(preimage, sig);
}

void
ircd::m::event::essential(json::iov &event,
                          const json::iov &contents,
                          const event::closure_iov_mutable &closure)
{
	const auto &type
	{
		event.at("type")
	};

	if(type == "m.room.aliases")
	{
		const json::iov::push _content{event,
		{
			"content", json::members
			{
				{ "aliases", contents.at("aliases") }
			}
		}};

		closure(event);
	}
	else if(type == "m.room.create")
	{
		const json::iov::push _content{event,
		{
			"content", json::members
			{
				{ "creator", contents.at("creator") }
			}
		}};

		closure(event);
	}
	else if(type == "m.room.history_visibility")
	{
		const json::iov::push _content{event,
		{
			"content", json::members
			{
				{ "history_visibility", contents.at("history_visibility") }
			}
		}};

		closure(event);
	}
	else if(type == "m.room.join_rules")
	{
		const json::iov::push _content{event,
		{
			"content", json::members
			{
				{ "join_rule", contents.at("join_rule") }
			}
		}};

		closure(event);
	}
	else if(type == "m.room.member")
	{
		const json::iov::push _content{event,
		{
			"content", json::members
			{
				{ "membership", contents.at("membership") }
			}
		}};

		closure(event);
	}
	else if(type == "m.room.power_levels")
	{
		const json::iov::push _content{event,
		{
			"content", json::members
			{
				{ "ban", contents.at("ban")                        },
				{ "events", contents.at("events")                  },
				{ "events_default", contents.at("events_default")  },
				{ "kick", contents.at("kick")                      },
				{ "redact", contents.at("redact")                  },
				{ "state_default", contents.at("state_default")    },
				{ "users", contents.at("users")                    },
				{ "users_default", contents.at("users_default")    },
			}
		}};

		closure(event);
	}
	else if(type == "m.room.redaction")
	{
		// This simply finds the redacts key and swaps it with jsundefined for
		// the scope's duration. The redacts key will still be present and
		// visible in the json::iov which is incorrect if directly serialized.
		// However, this iov is turned into a json::tuple (m::event) which ends
		// up being serialized for signing. That serialization is where the
		// jsundefined redacts value is ignored.
		auto &redacts{event.at("redacts")};
		json::value temp(std::move(redacts));
		redacts = json::value{};
		const unwind _{[&redacts, &temp]
		{
			redacts = std::move(temp);
		}};

		const json::iov::push _content
		{
			event, { "content", "{}" }
		};

		closure(event);
	}
	else
	{
		const json::iov::push _content
		{
			event, { "content", "{}" }
		};

		closure(event);
	}
}

ircd::m::event
ircd::m::essential(m::event event,
                   const mutable_buffer &contentbuf)
{
	const auto &type
	{
		json::at<"type"_>(event)
	};

	json::object &content
	{
		json::get<"content"_>(event)
	};

	mutable_buffer essential
	{
		contentbuf
	};

	if(type == "m.room.aliases")
	{
		content = json::stringify(essential, json::members
		{
			{ "aliases", content.at("aliases") }
		});
	}
	else if(type == "m.room.create")
	{
		content = json::stringify(essential, json::members
		{
			{ "creator", content.at("creator") }
		});
	}
	else if(type == "m.room.history_visibility")
	{
		content = json::stringify(essential, json::members
		{
			{ "history_visibility", content.at("history_visibility") }
		});
	}
	else if(type == "m.room.join_rules")
	{
		content = json::stringify(essential, json::members
		{
			{ "join_rule", content.at("join_rule") }
		});
	}
	else if(type == "m.room.member")
	{
		content = json::stringify(essential, json::members
		{
			{ "membership", content.at("membership") }
		});
	}
	else if(type == "m.room.power_levels")
	{
		content = json::stringify(essential, json::members
		{
			{ "ban", content.at("ban")                       },
			{ "events", content.at("events")                 },
			{ "events_default", content.at("events_default") },
			{ "kick", content.at("kick")                     },
			{ "redact", content.at("redact")                 },
			{ "state_default", content.at("state_default")   },
			{ "users", content.at("users")                   },
			{ "users_default", content.at("users_default")   },
		});
	}
	else if(type == "m.room.redaction")
	{
		json::get<"redacts"_>(event) = string_view{};
		content = "{}"_sv;
	}
	else
	{
		content = "{}"_sv;
	}

	json::get<"signatures"_>(event) = {};
	return event;
}

ircd::m::id::event
ircd::m::make_id(const event &event,
                 id::event::buf &buf)
{
	const crh::sha256::buf hash{event};
	return make_id(event, buf, hash);
}

ircd::m::id::event
ircd::m::make_id(const event &event,
                 id::event::buf &buf,
                 const const_buffer &hash)
{
	char readable[b58encode_size(sha256::digest_size)];
	const id::event ret
	{
		buf, b58encode(readable, hash), my_host()
	};

	buf.assigned(ret);
	return ret;
}

bool
ircd::m::check_id(const event &event)
noexcept
{
	if(!event.event_id)
		return false;

	const string_view &version
	{
		event.event_id.version()
	};

	return check_id(event, version);
}

bool
ircd::m::check_id(const event &event,
                  const string_view &room_version)
noexcept try
{
	const auto &version
	{
		room_version?: json::get<"event_id"_>(event)? "1": "4"
	};

	thread_local char buf[64];
	m::event::id check_id; switch(hash(version))
	{
		case "1"_:
		case "2"_:
			check_id = m::event::id{json::get<"event_id"_>(event)};
			break;

		case "3"_:
			check_id = m::event::id::v3{buf, event};
			break;

		case "4"_:
		case "5"_:
		default:
			check_id = m::event::id::v4{buf, event};
			break;
	}

	return event.event_id == check_id;
}
catch(...)
{
	assert(0);
	return false;
}

bool
ircd::m::before(const event &a,
                const event &b)
{
	const event::prev prev{b};
	return prev.prev_events_has(a.event_id);
}

bool
ircd::m::operator>=(const event &a, const event &b)
{
	//assert(json::get<"room_id"_>(a) == json::get<"room_id"_>(b));
	return json::get<"depth"_>(a) >= json::get<"depth"_>(b);
}

bool
ircd::m::operator<=(const event &a, const event &b)
{
	//assert(json::get<"room_id"_>(a) == json::get<"room_id"_>(b));
	return json::get<"depth"_>(a) <= json::get<"depth"_>(b);
}

bool
ircd::m::operator>(const event &a, const event &b)
{
	//assert(json::get<"room_id"_>(a) == json::get<"room_id"_>(b));
	return json::get<"depth"_>(a) > json::get<"depth"_>(b);
}

bool
ircd::m::operator<(const event &a, const event &b)
{
	//assert(json::get<"room_id"_>(a) == json::get<"room_id"_>(b));
	return json::get<"depth"_>(a) < json::get<"depth"_>(b);
}

bool
ircd::m::operator==(const event &a, const event &b)
{
	//assert(json::get<"room_id"_>(a) == json::get<"room_id"_>(b));
	return a.event_id == b.event_id;
}

bool
ircd::m::bad(const id::event &event_id)
{
	bool ret {false};
	index(event_id, std::nothrow, [&ret]
	(const event::idx &event_idx)
	{
		ret = event_idx == 0;
	});

	return ret;
}

size_t
ircd::m::count(const event::prev &prev)
{
	size_t ret{0};
	m::for_each(prev, [&ret](const event::id &event_id)
	{
		++ret;
	});

	return ret;
}

bool
ircd::m::good(const id::event &event_id)
{
	return bool(event_id) && index(event_id, std::nothrow) != 0;
}

bool
ircd::m::exists(const id::event &event_id,
                const bool &good)
{
	return good?
		m::good(event_id):
		m::exists(event_id);
}

bool
ircd::m::exists(const id::event &event_id)
{
	auto &column
	{
		dbs::event_idx
	};

	return bool(event_id) && has(column, event_id);
}

ircd::string_view
ircd::m::membership(const event &event)
{
	const json::object &content
	{
		json::get<"content"_>(event)
	};

	const json::string &content_membership
	{
		content.get("membership")
	};

	return content_membership;
}

size_t
ircd::m::degree(const event &event)
{
	return degree(event::prev{event});
}

size_t
ircd::m::degree(const event::prev &prev)
{
	size_t ret{0};
	json::for_each(prev, [&ret]
	(const auto &, const json::array &prevs)
	{
		ret += prevs.count();
	});

	return ret;
}

bool
ircd::m::my(const event &event)
{
	const auto &origin(json::get<"origin"_>(event));
	const auto &eid(event.event_id);
	return
		origin?
			my_host(origin):
		eid?
			my(event::id(eid)):
		false;
}

bool
ircd::m::my(const id::event &event_id)
{
	return self::host(event_id.host());
}

//
// event::event
//

ircd::m::event::event(const json::members &members)
:super_type
{
	members
}
,event_id
{
	defined(json::get<"event_id"_>(*this))?
		id{json::get<"event_id"_>(*this)}:
		id{},
}
{
}

ircd::m::event::event(const json::iov &members)
:event
{
	members,
	members.has("event_id")?
		id{members.at("event_id")}:
		id{}
}
{
}

ircd::m::event::event(const json::iov &members,
                      const id &id)
:super_type
{
	members
}
,event_id
{
	id
}
{
}

ircd::m::event::event(const json::object &source)
:super_type
{
	source
}
,event_id
{
	defined(json::get<"event_id"_>(*this))?
		id{json::get<"event_id"_>(*this)}:
		id{},
}
,source
{
	source
}
{
}

ircd::m::event::event(const json::object &source,
                      const keys &keys)
:super_type
{
	source, keys
}
,event_id
{
	defined(json::get<"event_id"_>(*this))?
		id{json::get<"event_id"_>(*this)}:
		id{},
}
,source
{
	source
}
{
}

ircd::m::event::event(id::buf &buf,
                      const json::object &source,
                      const string_view &version)
:event
{
	source,
	version == "1"?
		id{unquote(source.get("event_id"))}:
	version == "2"?
		id{unquote(source.get("event_id"))}:
	version == "3"?
		id{id::v3{buf, source}}:
	version == "4"?
		id{id::v4{buf, source}}:
		id{id::v4{buf, source}},
}
{
}

ircd::m::event::event(const json::object &source,
                      const id &event_id)
:super_type
{
	source
}
,event_id
{
	event_id
}
,source
{
	source
}
{
}

ircd::m::event::event(const json::object &source,
                      const id &event_id,
                      const keys &keys)
:super_type
{
	source, keys
}
,event_id
{
	event_id
}
,source
{
	source
}
{
}
