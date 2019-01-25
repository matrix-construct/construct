// Matrix Construct
//
// Copyright (C) Matrix Construct Developers, Authors & Contributors
// Copyright (C) 2016-2018 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

using namespace ircd::m;
using namespace ircd;

namespace ircd::m
{
	extern "C" void
	_essential__iov(json::iov &event,
	                const json::iov &contents,
	                const event::closure_iov_mutable &closure);

	extern "C" m::event &
	_essential(m::event &event,
               const mutable_buffer &contentbuf);
}

mapi::header
IRCD_MODULE
{
	"Matrix event library; modular components."
};

void
ircd::m::_essential__iov(json::iov &event,
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

ircd::m::event &
ircd::m::_essential(m::event &event,
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
			{ "aliases", unquote(content.at("aliases")) }
		});
	}
	else if(type == "m.room.create")
	{
		content = json::stringify(essential, json::members
		{
			{ "creator", unquote(content.at("creator")) }
		});
	}
	else if(type == "m.room.history_visibility")
	{
		content = json::stringify(essential, json::members
		{
			{ "history_visibility", unquote(content.at("history_visibility")) }
		});
	}
	else if(type == "m.room.join_rules")
	{
		content = json::stringify(essential, json::members
		{
			{ "join_rule", unquote(content.at("join_rule")) }
		});
	}
	else if(type == "m.room.member")
	{
		content = json::stringify(essential, json::members
		{
			{ "membership", unquote(content.at("membership")) }
		});
	}
	else if(type == "m.room.power_levels")
	{
		content = json::stringify(essential, json::members
		{
			{ "ban", unquote(content.at("ban"))                       },
			{ "events", unquote(content.at("events"))                 },
			{ "events_default", unquote(content.at("events_default")) },
			{ "kick", unquote(content.at("kick"))                     },
			{ "redact", unquote(content.at("redact"))                 },
			{ "state_default", unquote(content.at("state_default"))   },
			{ "users", unquote(content.at("users"))                   },
			{ "users_default", unquote(content.at("users_default"))   },
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

extern "C" void
pretty__event(std::ostream &s,
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

	const auto &auth_events{json::get<"auth_events"_>(event)};
	for(const json::array auth_event : auth_events)
	{
		s << std::setw(16) << std::right << "[auth event]"
		  << " :" << unquote(auth_event[0]);

		for(const auto &hash : json::object{auth_event[1]})
			s << " " << unquote(hash.first)
			  << ": " << unquote(hash.second);

		s << std::endl;
	}

	const auto &prev_states{json::get<"prev_state"_>(event)};
	for(const json::array prev_state : prev_states)
	{
		s << std::setw(16) << std::right << "[prev state]"
		  << " :" << unquote(prev_state[0]);

		for(const auto &hash : json::object{prev_state[1]})
			s << " " << unquote(hash.first)
			  << ": " << unquote(hash.second);

		s << std::endl;
	}

	const auto &prev_events{json::get<"prev_events"_>(event)};
	for(const json::array prev_event : prev_events)
	{
		s << std::setw(16) << std::right << "[prev_event]"
		  << " :" << unquote(prev_event[0]);

		for(const auto &hash : json::object{prev_event[1]})
			s << " " << unquote(hash.first)
			  << ": " << unquote(hash.second);

		s << std::endl;
	}

	if(!contents.empty())
		for(const json::object::member &content : contents)
			s << std::setw(16) << std::right << "[content]" << " :"
			  << std::setw(7) << std::left << reflect(json::type(content.second)) << " "
			  << std::setw(5) << std::right << size(string_view{content.second}) << " bytes "
			  << ':' << content.first
			  << std::endl;
}

extern "C" void
pretty_oneline__event(std::ostream &s,
                      const event &event,
                      const bool &content_keys)
{
	const auto out{[&s]
	(const string_view &key, auto&& val)
	{
		if(defined(json::value(val)))
			s << val << " ";
		else
			s << "* ";
	}};

	const string_view top_keys[]
	{
		"origin",
		"event_id",
		"sender",
	};

	if(defined(json::get<"room_id"_>(event)))
		s << json::get<"room_id"_>(event) << " ";
	else
		s << "* ";

	if(json::get<"depth"_>(event) != json::undefined_number)
		s << json::get<"depth"_>(event) << " :";
	else
		s << "* :";

	json::for_each(event, top_keys, out);

	const auto &auth_events{json::get<"auth_events"_>(event)};
	s << "A:" << auth_events.count() << " ";

	const auto &prev_states{json::get<"prev_state"_>(event)};
	s << "S:" << prev_states.count() << " ";

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

	out("type", json::get<"type"_>(event));

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
			string_view{}
	};

	out("content.membership", membership);
	out("redacts", json::get<"redacts"_>(event));

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
}

extern "C" void
pretty_msgline__event(std::ostream &s,
                      const event &event)
{
	s << json::get<"depth"_>(event) << " :";
	s << json::get<"type"_>(event) << " ";
	s << json::get<"sender"_>(event) << " ";
	s << json::get<"event_id"_>(event) << " ";

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
}

extern "C" void
pretty__prev(std::ostream &s,
             const event::prev &prev)
{
	const auto out{[&s]
	(const string_view &key, auto&& val)
	{
		if(json::defined(val))
			s << key << " :" << val << std::endl;
	}};

	const auto &auth_events{json::get<"auth_events"_>(prev)};
	for(const json::array auth_event : auth_events)
	{
		s << std::setw(16) << std::right << "[auth event]"
		  << " :" << unquote(auth_event[0]);

		for(const auto &hash : json::object{auth_event[1]})
			s << " " << unquote(hash.first)
			  << ": " << unquote(hash.second);

		s << std::endl;
	}

	const auto &prev_states{json::get<"prev_state"_>(prev)};
	for(const json::array prev_state : prev_states)
	{
		s << std::setw(16) << std::right << "[prev state]"
		  << " :" << unquote(prev_state[0]);

		for(const auto &hash : json::object{prev_state[1]})
			s << " " << unquote(hash.first)
			  << ": " << unquote(hash.second);

		s << std::endl;
	}

	const auto &prev_events{json::get<"prev_events"_>(prev)};
	for(const json::array prev_event : prev_events)
	{
		s << std::setw(16) << std::right << "[prev_event]"
		  << " :" << unquote(prev_event[0]);

		for(const auto &hash : json::object{prev_event[1]})
			s << " " << unquote(hash.first)
			  << ": " << unquote(hash.second);

		s << std::endl;
	}
}

extern "C" void
pretty_oneline__prev(std::ostream &s,
                     const event::prev &prev)
{
	const auto &auth_events{json::get<"auth_events"_>(prev)};
	s << "A[ ";
	for(const json::array auth_event : auth_events)
		s << unquote(auth_event[0]) << " ";
	s << "] ";

	const auto &prev_states{json::get<"prev_state"_>(prev)};
	s << "S[ ";
	for(const json::array prev_state : prev_states)
		s << unquote(prev_state[0]) << " ";
	s << "] ";

	const auto &prev_events{json::get<"prev_events"_>(prev)};
	s << "E[ ";
	for(const json::array prev_event : prev_events)
		s << unquote(prev_event[0]) << " ";
	s << "] ";
}
