// Matrix Construct
//
// Copyright (C) Matrix Construct Developers, Authors & Contributors
// Copyright (C) 2016-2019 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

std::ostream &
ircd::m::pretty_detailed(std::ostream &out,
                         const event &event,
                         const event::idx &event_idx)
{
	const bool cached
	{
		event_idx && m::cached(event_idx)
	};

	const auto cached_keys
	{
		event_idx?
			m::cached_keys(event_idx, m::event::keys::selection{}):
			m::event::keys::selection{m::event::keys::include{}}
	};

	const bool full_json
	{
		event_idx && has(m::dbs::event_json, byte_view<string_view>(event_idx))
	};

	out
	<< pretty(event)
	<< std::endl;

	if(event_idx)
		out
		<< std::setw(16) << std::right << "SEQUENCE" << "  "
		<< event_idx
		<< std::endl;

	if(event.source)
	{
		char pbuf[64];
		out
		<< std::setw(16) << std::right << "JSON SIZE" << "  "
		<< pretty(pbuf, iec(size(string_view{event.source})))
		<< std::endl;
	}

	if(cached || cached_keys.count())
	{
		out << std::setw(16) << std::right << "CACHED" << "  ";

		if(cached)
			out << " _json";

		for(const auto &key : m::event::keys{cached_keys})
			out << " " << key;

		out << std::endl;
	}

	if(m::room::auth::is_power_event(event))
		out
		<< std::setw(16) << std::right << "POWER EVENT" << "  "
		<< std::endl;

	const m::event::prev prev{event};
	const m::event::auth auth{event};
	if(auth.auth_events_count() || prev.prev_events_count())
		out
		<< std::setw(16) << std::right << "REFERENCES" << "  "
		<< (auth.auth_events_count() + prev.prev_events_count())
		<< std::endl;

	const m::event::refs &refs{event_idx};
	if(refs.count())
		out
		<< std::setw(16) << std::right << "REFERENCED BY" << "  "
		<< refs.count()
		<< std::endl;

	out << std::endl;
	for(size_t i(0); i < auth.auth_events_count(); ++i)
	{
		const m::event::id &id{auth.auth_event(i)};
		const m::event::fetch event{std::nothrow, id};
		if(!event.valid)
		{
			out
			<< "x-> AUTH        "
			<< id
			<< std::endl;
			continue;
		}

		out
		<< "--> AUTH        "
		<< " " << std::setw(9) << std::right << event.event_idx
		<< " " << pretty_oneline(event, false) << std::endl;
	}

	for(size_t i(0); i < prev.prev_events_count(); ++i)
	{
		const m::event::id &id{prev.prev_event(i)};
		const m::event::fetch event{std::nothrow, id};
		if(!event.valid)
		{
			out
			<< "x-> PREV        " << id
			<< std::endl;
			continue;
		}

		out
		<< "--> PREV        "
		<< " " << std::setw(9) << std::right << event.event_idx
		<< " " << pretty_oneline(event, false)
		<< std::endl;
	}

	if(event_idx)
		out
		<< std::setw(16) << std::left << "---"
		<< " " << std::setw(9) << std::right << event_idx
		<< " " << pretty_oneline(event, false)
		<< std::endl;

	const auto refcnt(refs.count());
	if(refcnt)
	{
		refs.for_each([&out]
		(const m::event::idx &idx, const auto &type)
		{
			const m::event::fetch event{idx};
			out
			<< "<-- " << std::setw(12) << std::left << trunc(reflect(type), 12)
			<< " " << std::setw(9) << std::right << idx
			<< " " << pretty_oneline(event, false)
			<< std::endl;
			return true;
		});
	}

	out << std::endl;
	if(event.source && !json::valid(event.source, std::nothrow))
		out
		<< std::setw(9) << std::left << "!!! ERROR" << "  "
		<< "JSON SOURCE INVALID"
		<< std::endl;

	const m::event::conforms conforms
	{
		event
	};

	if(!conforms.clean())
		out
		<< std::setw(9) << std::left << "!!! ERROR" << "  "
		<< conforms
		<< std::endl;

	if(!verify_hash(event))
	{
		char buf[512];
		out
		<< std::setw(9) << std::left << "!!! ERROR" << "  "
		<< "HASH MISMATCH :" << b64::encode_unpadded(buf, hash(event))
		<< std::endl;
	}

	{
		const auto &[authed, failmsg](m::room::auth::check_static(event));
		if(!authed)
			out
			<< std::setw(9) << std::left << "!!! ERROR" << "  "
			<< "STATICALLY UNAUTHORIZED :" << what(failmsg)
			<< std::endl;
	}

	{
		const auto &[authed, failmsg](m::room::auth::check_relative(event));
		if(!authed)
			out
			<< std::setw(9) << std::left << "!!! ERROR" << "  "
			<< "RELATIVELY UNAUTHORIZED :" << what(failmsg)
			<< std::endl;
	}

	{
		const auto &[authed, failmsg](m::room::auth::check_present(event));
		if(!authed)
			out
			<< std::setw(9) << std::left << "!!! ERROR" << "  "
			<< "PRESENTLY UNAUTHORIZED  :" << what(failmsg)
			<< std::endl;
	}

	try
	{
		if(!verify(event))
			out
			<< std::setw(9) << std::left << "!!! ERROR" << "  "
			<< "SIGNATURE FAILED"
			<< std::endl;
	}
	catch(const std::exception &e)
	{
			out
			<< std::setw(9) << std::left << "!!! ERROR" << "  "
			<< "SIGNATURE FAILED :" << e.what()
			<< std::endl;
	}

	return out;
}

std::ostream &
ircd::m::pretty_stateline(std::ostream &out,
                          const event &event,
                          const event::idx &event_idx)
{
	const room room
	{
		json::get<"room_id"_>(event)
	};

	const room::state &state
	{
		room
	};

	const bool active
	{
		event_idx?
			state.has(event_idx):
			false
	};

	const bool redacted
	{
		event_idx?
			bool(m::redacted(event_idx)):
			false
	};

	const bool power
	{
		m::room::auth::is_power_event(event)
	};

	const room::auth::passfail auth[]
	{
		event_idx?
			room::auth::check_static(event):
			room::auth::passfail{false, {}},

		event_idx && m::exists(event.event_id)?
			room::auth::check_relative(event):
			room::auth::passfail{false, {}},

		event_idx?
			room::auth::check_present(event):
			room::auth::passfail{false, {}},
	};

	char buf[32];
	const string_view flags
	{
		fmt::sprintf
		{
			buf, "%c %c%c%c%c%c",

			active? '*' : ' ',
			power? '@' : ' ',
			redacted? 'R' : ' ',

			std::get<bool>(auth[0]) && !std::get<std::exception_ptr>(auth[0])? ' ':
			!std::get<bool>(auth[0]) && std::get<std::exception_ptr>(auth[0])? 'X': '?',

			std::get<bool>(auth[1]) && !std::get<std::exception_ptr>(auth[1])? ' ':
			!std::get<bool>(auth[1]) && std::get<std::exception_ptr>(auth[1])? 'X': '?',

			std::get<bool>(auth[2]) && !std::get<std::exception_ptr>(auth[2])? ' ':
			!std::get<bool>(auth[2]) && std::get<std::exception_ptr>(auth[2])? 'X': '?',
		}
	};

	const auto &type
	{
		json::get<"type"_>(event)
	};

	const auto &state_key
	{
		json::get<"state_key"_>(event)
	};

	const auto &depth
	{
		json::get<"depth"_>(event)
	};

	const json::string &content
	{
		type == "m.room.member"?
			m::membership(event):

		type == "m.room.history_visibility"?
			json::get<"content"_>(event).get("history_visibility"):

		type == "m.room.join_rules"?
			json::get<"content"_>(event).get("join_rule"):

		type == "m.room.name"?
			json::get<"content"_>(event).get("name"):

		type == "m.room.canonical_alias"?
			json::get<"content"_>(event).get("alias"):

		type == "m.room.avatar"?
			json::get<"content"_>(event).get("url"):

		json::string{}
	};

	char smbuf[48];
	if(event.event_id.version() == "1")
	{
		out
		<< smalldate(smbuf, json::get<"origin_server_ts"_>(event) / 1000L)
		<< std::right << " "
		<< std::setw(9) << json::get<"depth"_>(event)
		<< std::right << " [ "
		<< std::setw(30) << type
		<< std::left << " | "
		<< std::setw(50) << state_key
		<< std::left << " ]" << flags << " "
		<< std::setw(10) << event_idx
		<< std::left << "  "
		<< std::setw(72) << string_view{event.event_id}
		<< std::left << ' '
		<< std::left << trunc(content, 80)
		;
	} else {
		out
		<< std::left
		<< smalldate(smbuf, json::get<"origin_server_ts"_>(event) / 1000L)
		<< ' '
		<< string_view{event.event_id}
		<< std::right << " "
		<< std::setw(9) << json::get<"depth"_>(event)
		<< std::right << " [ "
		<< std::setw(40) << type
		<< std::left << " | "
		<< std::setw(56) << state_key
		<< std::left << " ]" << flags << " "
		<< std::setw(10) << event_idx
		<< ' '
		<< std::left << trunc(content, 80)
		;
	}

	if(std::get<1>(auth[0]))
		out << ":" << trunc(what(std::get<1>(auth[0])), 72);

	out << std::endl;
	return out;
}

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

	if(!json::get<"event_id"_>(event) && event.event_id)
		s << std::setw(16) << std::right << "(event_id)" << " :"
		  << string_view{event.event_id}
		  << std::endl;

	json::for_each(event, top_keys, out);

	if(json::get<"type"_>(event) == "m.room.member")
		s << std::setw(16) << std::right << "membership" << " :"
		  << m::membership(event)
		  << std::endl;

	const auto &ts{json::get<"origin_server_ts"_>(event)};
	{
		char buf[128];
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
	if(defined(json::get<"room_id"_>(event)))
		s << json::get<"room_id"_>(event) << ' ';
	else
		s << "* ";

	char sdbuf[48];
	if(event.event_id && event.event_id.version() != "1")
		s << event.event_id << ' ';
	else if(!event.event_id) try
	{
		s << m::event::id::v4{sdbuf, event} << ' ';
	}
	catch(const std::exception &e)
	{
		s << "$[" << e.what() << "] ";
	}

	if(json::get<"origin_server_ts"_>(event) != json::undefined_number)
		s << smalldate(sdbuf, json::get<"origin_server_ts"_>(event) / 1000L) << ' ';
	else
		s << "* ";

	if(json::get<"depth"_>(event) != json::undefined_number)
		s << json::get<"depth"_>(event) << ' ';
	else
		s << "* ";

	const m::event::auth auth(event);
	for(size_t i(0); i < auth.auth_events_count(); ++i)
		s << 'A';

	const m::event::prev prev(event);
	for(size_t i(0); i < prev.prev_events_count(); ++i)
		s << 'P';

	if(auth.auth_events_count() || prev.prev_events_count())
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
ircd::m::pretty_msgline(const event &event,
                        const int &fmt)
{
	std::string ret;
	std::stringstream s;
	pubsetbuf(s, ret, 4096);
	pretty_msgline(s, event, fmt);
	resizebuf(s, ret);
	return ret;
}

std::ostream &
ircd::m::pretty_msgline(std::ostream &s,
                        const event &event,
                        const int &fmt)
{
	const bool text_only
	(
		fmt & 1
	);

	if(!text_only)
	{
		s << json::get<"depth"_>(event) << ' ';

		char sdbuf[48];
		if(json::get<"origin_server_ts"_>(event) != json::undefined_number)
			s << smalldate(sdbuf, json::get<"origin_server_ts"_>(event) / 1000L) << ' ';

		s << event.event_id << ' ';
		s << json::get<"sender"_>(event) << ' ';

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
	}

	const json::object &content
	{
		json::get<"content"_>(event)
	};

	switch(hash(json::get<"type"_>(event)))
	{
		case "m.room.message"_:
		{
			const json::string msgtype
			{
				content["msgtype"]
			};

			const json::string body
			{
				content["body"]
			};

			if(!text_only)
				s << msgtype << ' ';
			else if(msgtype != "m.text")
				break;

			s << body;
			break;
		}

		default:
			if(text_only)
				break;

			s << string_view{content};
			break;
	}

	return s;
}

std::string
ircd::m::pretty(const event::auth &auth)
{
	std::string ret;
	std::stringstream s;
	pubsetbuf(s, ret, 4096);
	pretty(s, auth);
	resizebuf(s, ret);
	return ret;
}

std::ostream &
ircd::m::pretty(std::ostream &s,
                const event::auth &auth)
{
	for(size_t i(0); i < auth.auth_events_count(); ++i)
	{
		const auto &[event_id, ref_hash]
		{
			auth.auth_events(i)
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

	return s;
}

std::ostream &
ircd::m::pretty_oneline(std::ostream &s,
                        const event::auth &auth)
{
	const auto &auth_events{json::get<"auth_events"_>(auth)};
	s << "A[ ";
	for(const json::array auth_event : auth_events)
		s << json::string(auth_event[0]) << ' ';
	s << "] ";

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
	const auto &prev_events{json::get<"prev_events"_>(prev)};
	s << "E[ ";
	for(const json::array prev_event : prev_events)
		s << json::string(prev_event[0]) << ' ';
	s << "] ";

	return s;
}
