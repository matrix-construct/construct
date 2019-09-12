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
		"event_id",
		"room_id",
		"sender",
		"origin",
		"depth",
		"type",
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
		  << json::string(hash.second)
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
                        const int &fmt)
{
	std::string ret;
	std::stringstream s;
	pubsetbuf(s, ret, 4096);
	pretty_oneline(s, event, fmt);
	resizebuf(s, ret);
	return ret;
}

std::ostream &
ircd::m::pretty_oneline(std::ostream &s,
                        const event &event,
                        const int &fmt)
{
	thread_local char sdbuf[48];

	if(defined(json::get<"room_id"_>(event)))
		s << json::get<"room_id"_>(event) << ' ';
	else
		s << "* ";

	if(event.event_id && event.event_id.version() != "1")
		s << event.event_id << ' ';
	else if(!event.event_id)
		s << m::event::id::v4{sdbuf, event} << ' ';

	if(json::get<"origin_server_ts"_>(event) != json::undefined_number)
		s << smalldate(sdbuf, json::get<"origin_server_ts"_>(event) / 1000L) << ' ';
	else
		s << "* ";

	if(json::get<"depth"_>(event) != json::undefined_number)
		s << json::get<"depth"_>(event) << ' ';
	else
		s << "* ";

	const m::event::prev prev(event);
	for(size_t i(0); i < prev.auth_events_count(); ++i)
		s << 'A';

	for(size_t i(0); i < prev.prev_events_count(); ++i)
		s << 'P';

	if(prev.auth_events_count() || prev.prev_events_count())
		s << ' ';

	if(event.event_id && event.event_id.version() == "1")
		s << event.event_id << ' ';

	if(fmt >= 2)
	{
		const auto &hashes{json::get<"hashes"_>(event)};
		s << "[ ";
		for(const auto &hash : hashes)
			s << hash.first << ' ';
		s << "] ";

		const auto &signatures{json::get<"signatures"_>(event)};
		s << "[ ";
		for(const auto &signature : signatures)
		{
			s << signature.first << "[ ";
			for(const auto &key : json::object{signature.second})
				s << key.first << ' ';

			s << "] ";
		}
		s << "] ";
	}

	if(defined(json::get<"type"_>(event)))
		s << json::get<"type"_>(event) << ' ';
	else
		s << "* ";

	const auto &state_key
	{
		json::get<"state_key"_>(event)
	};

	if(defined(state_key) && empty(state_key))
		s << "\"\"" << ' ';
	else if(defined(state_key))
		s << state_key << ' ';
	else
		s << "*" << ' ';

	const string_view &membership
	{
		json::get<"type"_>(event) == "m.room.member"?
			m::membership(event):
			"*"_sv
	};

	s << membership << ' ';

	if(defined(json::get<"redacts"_>(event)))
		s << json::get<"redacts"_>(event) << ' ';
	else
		s << "* ";

	if(defined(json::get<"origin"_>(event)) && defined(json::get<"sender"_>(event)))
		if(at<"origin"_>(event) != user::id(at<"sender"_>(event)).host())
			s << ':' << json::get<"origin"_>(event) << ' ';

	if(defined(json::get<"sender"_>(event)))
		s << json::get<"sender"_>(event) << ' ';
	else
		s << "@*:* ";

	const json::object &contents
	{
		fmt >= 1?
			json::get<"content"_>(event):
			json::object{}
	};

	if(!contents.empty())
	{
		s << "+" << string_view{contents}.size() << " bytes :";
		for(const auto &content : contents)
			s << content.first << ' ';
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
	s << json::get<"type"_>(event) << ' ';
	s << json::get<"sender"_>(event) << ' ';
	s << event.event_id << ' ';

	const auto &state_key
	{
		json::get<"state_key"_>(event)
	};

	if(defined(state_key) && empty(state_key))
		s << "\"\"" << ' ';
	else if(defined(state_key))
		s << state_key << ' ';
	else
		s << "*" << ' ';

	const json::object &content
	{
		json::get<"content"_>(event)
	};

	switch(hash(json::get<"type"_>(event)))
	{
		case "m.room.message"_:
			s << json::string(content.get("msgtype")) << ' ';
			s << json::string(content.get("body")) << ' ';
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
		{
			s << ' ' << json::string(algorithm);
			if(digest)
				s << ": " << json::string(digest);
		}

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
		{
			s << ' ' << json::string(algorithm);
			if(digest)
				s << ": " << json::string(digest);
		}

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
		s << json::string(auth_event[0]) << ' ';
	s << "] ";

	const auto &prev_events{json::get<"prev_events"_>(prev)};
	s << "E[ ";
	for(const json::array prev_event : prev_events)
		s << json::string(prev_event[0]) << ' ';
	s << "] ";

	return s;
}

///////////////////////////////////////////////////////////////////////////////
//
// event/conforms.h
//

namespace ircd::m::vm
{
	extern m::hookfn<eval &> conform_check_event_id;
	extern m::hookfn<eval &> conform_check_origin;
	extern m::hookfn<eval &> conform_check_size;
	extern m::hookfn<eval &> conform_report;
}

/// Check if event_id is sufficient for the room version.
decltype(ircd::m::vm::conform_check_event_id)
ircd::m::vm::conform_check_event_id
{
	{
		{ "_site", "vm.conform" }
	},
	[](const m::event &event, eval &eval)
	{
		// Don't care about EDU's on this hook
		if(!event.event_id)
			return;

		// Conditions for when we don't care if the event_id conforms. This
		// hook only cares if the event_id is sufficient for the version, and
		// we don't care about the early matrix versions with mxids here.
		const bool unaffected
		{
			!eval.room_version
			|| eval.room_version == "0"
			|| eval.room_version == "1"
			|| eval.room_version == "2"
		};

		if(eval.room_version == "3")
			if(!event::id::v3::is(event.event_id))
				throw error
				{
					fault::INVALID, "Event ID %s is not sufficient for version 3 room.",
					string_view{event.event_id}
				};

		// note: we check v4 format for all other room versions, including "4"
		if(!unaffected && eval.room_version != "3")
			if(!event::id::v4::is(event.event_id))
				throw error
				{
					fault::INVALID, "Event ID %s in a version %s room is not a version 4 Event ID.",
					string_view{event.event_id},
					eval.room_version,
				};
	}
};

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
		if(eval.opts && !eval.opts->conforming)
			return;

		if(unlikely(eval.copts && !my_host(at<"origin"_>(event))))
			throw error
			{
				fault::INVALID, "Issuing event for origin: %s", at<"origin"_>(event)
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
	"MISMATCH_EVENT_ID",
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

	if(!has(INVALID_OR_MISSING_EVENT_ID))
		if(!m::check_id(e))
			set(MISMATCH_EVENT_ID);

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
		for(size_t i(0); i < prev.auth_events_count(); ++i)
			if(prev.auth_event(i) == json::get<"event_id"_>(e))
				set(SELF_AUTH_EVENT);

		for(size_t i(0); i < prev.prev_events_count(); ++i)
			if(prev.prev_event(i) == json::get<"event_id"_>(e))
				set(SELF_PREV_EVENT);
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

bool
ircd::m::prefetch(const event::id &event_id,
                  const event::fetch::opts &opts)
{
	return prefetch(index(event_id), opts);
}

bool
ircd::m::prefetch(const event::id &event_id,
                  const string_view &key)
{
	return prefetch(index(event_id), key);
}

bool
ircd::m::prefetch(const event::idx &event_idx,
                  const event::fetch::opts &opts)
{
	if(event::fetch::should_seek_json(opts))
		return db::prefetch(dbs::event_json, byte_view<string_view>{event_idx});

	const event::keys keys
	{
		opts.keys
	};

	const vector_view<const string_view> cols
	{
		keys
	};

	bool ret{false};
	for(const auto &col : cols)
		if(col)
			ret |= prefetch(event_idx, col);

	return ret;
}

bool
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

	return db::prefetch(column, byte_view<string_view>{event_idx});
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
	return get(std::nothrow, index(event_id, std::nothrow), key, buf);
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
	return get(std::nothrow, index(event_id, std::nothrow), key, closure);
}

bool
ircd::m::get(std::nothrow_t,
             const event::idx &event_idx,
             const string_view &key,
             const event::fetch::view_closure &closure)
{
	const auto &column_idx
	{
		json::indexof<event>(key)
	};

	auto &column
	{
		dbs::event_column.at(column_idx)
	};

	if(!event_idx)
		return false;

	const string_view &column_key
	{
		byte_view<string_view>{event_idx}
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
			value = json::string(value);

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
	return seek(fetch, event_idx, m::event::id{}, std::nothrow);
}

bool
ircd::m::seek(event::fetch &fetch,
              const event::idx &event_idx,
              const event::id &event_id,
              std::nothrow_t)
{
	fetch.event_idx = event_idx;
	fetch.event_id_buf = event_id?
		event::id::buf{event_id}:
		event::id::buf{};

	if(!event_idx)
	{
		fetch.valid = false;
		return fetch.valid;
	}

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
		!event_id_buf && source.has("event_id")
	};

	const auto event_id
	{
		source_event_id?
			id(json::string(source.at("event_id"))):
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
		!empty(json::get<"event_id"_>(event))?
			id{json::get<"event_id"_>(event)}:
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
{
	return index(event.event_id, std::nothrow);
}

ircd::m::event::idx
ircd::m::index(const event::id &event_id)
{
	assert(event_id);
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
// event/horizon.h
//

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
const
{
	return count(dbs::ref(-1));
}

size_t
ircd::m::event::refs::count(const dbs::ref &type)
const
{
	assert(idx);
	size_t ret(0);
	for_each(type, [&ret]
	(const event::idx &, const dbs::ref &)
	{
		++ret;
		return true;
	});

	return ret;
}

bool
ircd::m::event::refs::has(const event::idx &idx)
const
{
	return has(dbs::ref(-1), idx);
}

bool
ircd::m::event::refs::has(const dbs::ref &type)
const
{
	return !for_each(type, [&type]
	(const event::idx &, const dbs::ref &ref)
	{
		assert(ref == type);
		return false;
	});
}

bool
ircd::m::event::refs::has(const dbs::ref &type,
                          const event::idx &idx)
const
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

size_t
ircd::m::event::prev::prev_events_exist()
const
{
	size_t ret(0);
	for(size_t i(0); i < prev_events_count(); ++i)
		ret += prev_event_exists(i);

	return ret;
}

size_t
ircd::m::event::prev::auth_events_exist()
const
{
	size_t ret(0);
	for(size_t i(0); i < auth_events_count(); ++i)
		ret += auth_event_exists(i);

	return ret;
}

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
		json::at<"auth_events"_>(*this).at(idx)
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
		json::at<"prev_events"_>(*this).at(idx)
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

	const json::string &hash
	{
		object.at("sha256")
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
	for(const auto &[host, sig] : json::get<"signatures"_>(event_))
		if(!my_host(json::string(host)))
			sigs.at(i++) = { host, sig };

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

	for(const auto &[host, sig] : origin_sigs)
		if(verify(event, origin, json::string(host)))
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
			b64decode(buf, json::string(origin_sigs.at(keyid)));
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
try
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
catch(const std::exception &e)
{
	log::derror
	{
		log, "Error while isolating essential keys (redaction algorithm) :%s",
		e.what(),
	};

	throw;
}

ircd::m::event
ircd::m::essential(m::event event,
                   const mutable_buffer &contentbuf)
try
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
		if(content.has("aliases"))
			content = json::stringify(essential, json::members
			{
				{ "aliases", content.at("aliases") }
			});
	}
	else if(type == "m.room.create")
	{
		if(content.has("creator"))
			content = json::stringify(essential, json::members
			{
				{ "creator", content.at("creator") }
			});
	}
	else if(type == "m.room.history_visibility")
	{
		if(content.has("history_visibility"))
			content = json::stringify(essential, json::members
			{
				{ "history_visibility", content.at("history_visibility") }
			});
	}
	else if(type == "m.room.join_rules")
	{
		if(content.has("join_rule"))
			content = json::stringify(essential, json::members
			{
				{ "join_rule", content.at("join_rule") }
			});
	}
	else if(type == "m.room.member")
	{
		if(content.has("membership"))
			content = json::stringify(essential, json::members
			{
				{ "membership", content.at("membership") }
			});
	}
	else if(type == "m.room.power_levels")
	{
		json::stack out{essential};
		json::stack::object top{out};

		if(content.has("ban"))
			json::stack::member
			{
				top, "ban", content.at("ban")
			};

		if(content.has("events"))
			json::stack::member
			{
				top, "events", content.at("events")
			};

		if(content.has("events_default"))
			json::stack::member
			{
				top, "events_default", content.at("events_default")
			};

		if(content.has("kick"))
			json::stack::member
			{
				top, "kick", content.at("kick")
			};

		if(content.has("redact"))
			json::stack::member
			{
				top, "redact", content.at("redact")
			};

		if(content.has("state_default"))
			json::stack::member
			{
				top, "state_default", content.at("state_default")
			};

		if(content.has("users"))
			json::stack::member
			{
				top, "users", content.at("users")
			};

		if(content.has("users_default"))
			json::stack::member
			{
				top, "users_default", content.at("users_default")
			};

		top.~object();
		content = out.completed();
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
catch(const std::exception &e)
{
	log::derror
	{
		log, "Error while isolating essential keys (redaction algorithm) :%s",
		e.what(),
	};

	throw;
}

ircd::m::id::event
ircd::m::make_id(const event &event,
                 const string_view &version,
                 id::event::buf &buf)
{
	if(version == "1" || version == "2")
	{
		const crh::sha256::buf hash{event};
		return make_id(event, version, buf, hash);
	}

	if(version == "3")
		return event::id::v3
		{
			buf, event
		};

	return event::id::v4
	{
		buf, event
	};
}

ircd::m::id::event
ircd::m::make_id(const event &event,
                 const string_view &version,
                 id::event::buf &buf,
                 const const_buffer &hash)
{
	char readable[b64encode_size(sha256::digest_size)];

	if(version == "1" || version == "2")
	{
		const id::event ret
		{
			buf, b64tob64url(readable, b64encode_unpadded(readable, hash)), my_host()
		};

		buf.assigned(ret);
		return ret;
	}
	else if(version == "3")
	{
		const id::event ret
		{
			buf, b64encode_unpadded(readable, hash), string_view{}
		};

		buf.assigned(ret);
		return ret;
	}

	const id::event ret
	{
		buf, b64tob64url(readable, b64encode_unpadded(readable, hash)), string_view{}
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
	assert(event.event_id);
	const auto &version
	{
		room_version?: event.event_id.version()
	};

	char buf[64];
	const event::id &check_id
	{
		version == "1" || version == "2"?
			event::id{json::get<"event_id"_>(event)}:

		version == "3"?
			event::id{event::id::v3{buf, event}}:

		event::id{event::id::v4{buf, event}}
	};

	return event.event_id == check_id;
}
catch(const std::exception &e)
{
	log::error
	{
		"m::check_id() :%s", e.what()
	};

	return false;
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
		return true;
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

	const string_view &membership
	{
		json::get<"membership"_>(event)
	};

	if(membership)
		return membership;

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
	const auto &sender(json::get<"sender"_>(event));
	const auto &eid(event.event_id);
	return
		origin?
			my_host(origin):
		sender?
			my_host(user::id(sender).host()):
		eid?
			my(event::id(eid)):
		false;
}

bool
ircd::m::my(const id::event &event_id)
{
	assert(event_id.host());
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
,source
{
	source
}
,event_id
{
	defined(json::get<"event_id"_>(*this))?
		id{json::get<"event_id"_>(*this)}:
		id{},
}
{
}

ircd::m::event::event(const json::object &source,
                      const keys &keys)
:super_type
{
	source, keys
}
,source
{
	source
}
,event_id
{
	defined(json::get<"event_id"_>(*this))?
		id{json::get<"event_id"_>(*this)}:
		id{},
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
		id{json::string(source.get("event_id"))}:
	version == "2"?
		id{json::string(source.get("event_id"))}:
	version == "3"?
		id{id::v3{buf, source}}:
	version == "4"?
		id{id::v4{buf, source}}:
	source.has("event_id")?
		id{json::string(source.at("event_id"))}:
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
,source
{
	source
}
,event_id
{
	event_id?
		event_id:
	defined(json::get<"event_id"_>(*this))?
		id{json::get<"event_id"_>(*this)}:
		id{},
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
,source
{
	source
}
,event_id
{
	event_id?
		event_id:
	defined(json::get<"event_id"_>(*this))?
		id{json::get<"event_id"_>(*this)}:
		id{},
}
{
}
