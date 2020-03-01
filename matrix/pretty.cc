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
IRCD_MODULE_EXPORT
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

	thread_local char smbuf[48];
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
		<< std::left << " "
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
		;
	}

	if(std::get<1>(auth[0]))
		out << ":" << trunc(what(std::get<1>(auth[0])), 72);

	out << std::endl;
	return out;
}

std::string
IRCD_MODULE_EXPORT
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
IRCD_MODULE_EXPORT
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
IRCD_MODULE_EXPORT
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
IRCD_MODULE_EXPORT
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
IRCD_MODULE_EXPORT
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
IRCD_MODULE_EXPORT
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
IRCD_MODULE_EXPORT
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
IRCD_MODULE_EXPORT
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
IRCD_MODULE_EXPORT
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
