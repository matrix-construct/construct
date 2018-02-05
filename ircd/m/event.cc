// Matrix Construct
//
// Copyright (C) Matrix Construct Developers, Authors & Contributors
// Copyright (C) 2016-2018 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

#include <ircd/m/m.h>

decltype(ircd::m::event::column)
ircd::m::event::column
{};

decltype(ircd::m::event::events)
ircd::m::event::events
{};

//
// init
//

ircd::m::event::init::init()
{
	// Open the events database
	event::events = std::make_shared<database>("events"s, ""s, event::description);

	// Cache the columns for the event tuple in order for constant time lookup
	std::array<string_view, event::size()> keys;      //TODO: why did this happen?
	_key_transform(event{}, begin(keys), end(keys));  //TODO: how did this happen?
	for(size_t i(0); i < keys.size(); ++i)
		event::column[i] = db::column
		{
			*event::events, keys[i]
		};
}

ircd::m::event::init::~init()
noexcept
{
	// Columns have to be closed before DB closes
	for(auto &column : event::column)
		column = {};

	// Close DB (if no other ref)
	event::events = {};
}

//
// misc
//

ircd::m::id::event
ircd::m::event_id(const event &event,
                  id::event::buf &buf)
{
	const crh::sha256::buf hash{event};
	return event_id(event, buf, hash);
}

ircd::m::id::event
ircd::m::event_id(const event &event,
                  id::event::buf &buf,
                  const const_buffer &hash)
{
	char readable[b58encode_size(sha256::digest_size)];
	return id::event
	{
		buf, b58encode(readable, hash), my_host()
	};
}

ircd::m::id::event
ircd::m::event_id(const event &event)
{
	return at<"event_id"_>(event);
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

void
ircd::m::for_each(const event::prev &prev,
                  const std::function<void (const event::id &)> &closure)
{
	json::for_each(prev, [&closure]
	(const auto &key, const json::array &prevs)
	{
		for(const json::array &prev : prevs)
		{
			const event::id &id{unquote(prev[0])};
			closure(id);
		}
	});
}

std::string
ircd::m::pretty(const event::prev &prev)
{
	std::string ret;
	std::stringstream s;
	pubsetbuf(s, ret, 2048);

	const auto out{[&s]
	(const string_view &key, auto&& val)
	{
		if(json::defined(val))
			s << key << ": " << val << std::endl;
	}};

	const auto &auth_events{json::get<"auth_events"_>(prev)};
	for(const json::array auth_event : auth_events)
		out("auth_event", unquote(auth_event[0]));

	const auto &prev_states{json::get<"prev_state"_>(prev)};
	for(const json::array prev_state : prev_states)
		out("prev_state", unquote(prev_state[0]));

	const auto &prev_events{json::get<"prev_events"_>(prev)};
	for(const json::array prev_event : prev_events)
		out("prev_event", unquote(prev_event[0]));

	resizebuf(s, ret);
	return ret;
}

std::string
ircd::m::pretty_oneline(const event::prev &prev)
{
	std::string ret;
	std::stringstream s;
	pubsetbuf(s, ret, 1024);

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

	resizebuf(s, ret);
	return ret;
}

std::string
ircd::m::pretty(const event &event)
{
	std::string ret;
	std::stringstream s;
	pubsetbuf(s, ret, 2048);

	const auto out{[&s]
	(const string_view &key, auto&& val)
	{
		if(json::defined(val))
			s << std::setw(16) << std::right << key << ": " << val << std::endl;
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
		"membership",
	};

	json::for_each(event, top_keys, out);

	const auto &ts{json::get<"origin_server_ts"_>(event)};
	{
		thread_local char buf[128];
		s << std::setw(16) << std::right << "origin_server_ts" << ": "
		  << timef(buf, ts / 1000L, localtime)
		  << " (" << ts << ")"
		  << std::endl;
	}

	const auto &hashes{json::get<"hashes"_>(event)};
	for(const auto &hash : hashes)
	{
		s << std::setw(16) << std::right << "[hash]" << ": "
		  << hash.first
		  << " "
		  << unquote(hash.second)
		  << std::endl;
	}

	const auto &signatures{json::get<"signatures"_>(event)};
	for(const auto &signature : signatures)
	{
		s << std::setw(16) << std::right << "[signature]" << ": "
		  << signature.first << " ";

		for(const auto &key : json::object{signature.second})
			s << key.first << " ";

		s << std::endl;
	}

	const json::object &contents{json::get<"content"_>(event)};
	if(!contents.empty())
	{
		s << std::setw(16) << std::right << "[content]" << ": ";
		for(const auto &content : contents)
			s << content.first << ", ";
		s << std::endl;
	}

	const auto &auth_events{json::get<"auth_events"_>(event)};
	for(const json::array auth_event : auth_events)
		out("[auth_event]", unquote(auth_event[0]));

	const auto &prev_states{json::get<"prev_state"_>(event)};
	for(const json::array prev_state : prev_states)
		out("[prev_state]", unquote(prev_state[0]));

	const auto &prev_events{json::get<"prev_events"_>(event)};
	for(const json::array prev_event : prev_events)
		out("[prev_event]", unquote(prev_event[0]));

	resizebuf(s, ret);
	return ret;
}

std::string
ircd::m::pretty_oneline(const event &event)
{
	std::string ret;
	std::stringstream s;
	pubsetbuf(s, ret, 1024);

	const auto out{[&s]
	(const string_view &key, auto&& val)
	{
		if(defined(val))
			s << val << " ";
		else
			s << "* ";
	}};

	const string_view top_keys[]
	{
		"origin",
		"event_id",
		"room_id",
		"sender",
		"depth",
	};

	s << ':';
	json::for_each(event, top_keys, out);

	const auto &auth_events{json::get<"auth_events"_>(event)};
	s << "pa:" << auth_events.count() << " ";

	const auto &prev_states{json::get<"prev_state"_>(event)};
	s << "ps:" << prev_states.count() << " ";

	const auto &prev_events{json::get<"prev_events"_>(event)};
	s << "pe:" << prev_events.count() << " ";

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

	out("membership", json::get<"membership"_>(event));

	const json::object &contents{json::get<"content"_>(event)};
	if(!contents.empty())
	{
		s << "+" << string_view{contents}.size() << " bytes :";
		for(const auto &content : contents)
			s << content.first << " ";
	}

	resizebuf(s, ret);
	return ret;
}

//
// event
//

ircd::m::event::event(const id &id,
                      const mutable_buffer &buf)
{
	assert(events);

	db::gopts opts;
	opts.snapshot = database::snapshot{*events};
	for(size_t i(0); i < column.size(); ++i)
	{
		const db::cell cell
		{
			column[i], id, opts
		};

		db::assign(*this, cell, id);
	}

	const json::object obj
	{
		string_view{data(buf), json::print(buf, *this)}
	};

	new (this) m::event(obj);
}

ircd::m::event::event(fetch &tab)
{
	io::acquire(tab);
}

//
// Database descriptors
//

const ircd::database::descriptor
events_event_id_descriptor
{
	// name
	"event_id",

	// explanation
	R"(### protocol note:

	10.1
	The id of event.

	10.4
	MUST NOT exceed 255 bytes.

	### developer note:
	key is event_id. This is redundant data but we have to have it for now.
	)",

	// typing (key, value)
	{
		typeid(ircd::string_view), typeid(ircd::string_view)
	}
};

const ircd::database::descriptor
events_type_descriptor
{
	// name
	"type",

	// explanation
	R"(### protocol note:

	10.1
	The type of event. This SHOULD be namespaced similar to Java package naming conventions
	e.g. 'com.example.subdomain.event.type'.

	10.4
	MUST NOT exceed 255 bytes.

	### developer note:
	key is event_id
	)",

	// typing (key, value)
	{
		typeid(ircd::string_view), typeid(ircd::string_view)
	}
};

const ircd::database::descriptor
events_content_descriptor
{
	// name
	"content",

	// explanation
	R"(### protocol note:

	10.1
	The fields in this object will vary depending on the type of event. When interacting
	with the REST API, this is the HTTP body.

	### developer note:
	Since events must not exceed 65 KB the maximum size for the content is the remaining
	space after all the other fields for the event are rendered.

	key is event_id
	)",

	// typing (key, value)
	{
		typeid(ircd::string_view), typeid(ircd::string_view)
	}
};

const ircd::database::descriptor
events_room_id_descriptor
{
	// name
	"room_id",

	// explanation
	R"(### protocol note:

	10.2 (apropos room events)
	Required. The ID of the room associated with this event.

	10.4
	MUST NOT exceed 255 bytes.

	### developer note:
	key is event_id
	)",

	// typing (key, value)
	{
		typeid(ircd::string_view), typeid(ircd::string_view)
	}
};

const ircd::database::descriptor
events_sender_descriptor
{
	// name
	"sender",

	// explanation
	R"(### protocol note:

	10.2 (apropos room events)
	Required. Contains the fully-qualified ID of the user who sent this event.

	10.4
	MUST NOT exceed 255 bytes.

	### developer note:
	key is event_id
	)",

	// typing (key, value)
	{
		typeid(ircd::string_view), typeid(ircd::string_view)
	}
};

const ircd::database::descriptor
events_state_key_descriptor
{
	// name
	"state_key",

	// explanation
	R"(### protocol note:

	10.3 (apropos room state events)
	A unique key which defines the overwriting semantics for this piece of room state.
	This value is often a zero-length string. The presence of this key makes this event a
	State Event. The key MUST NOT start with '_'.

	10.4
	MUST NOT exceed 255 bytes.

	### developer note:
	key is event_id
	)",

	// typing (key, value)
	{
		typeid(ircd::string_view), typeid(ircd::string_view)
	}
};

const ircd::database::descriptor
events_origin_descriptor
{
	// name
	"origin",

	// explanation
	R"(### protocol note:

	FEDERATION 4.1
	DNS name of homeserver that created this PDU

	### developer note:
	key is event_id
	)",

	// typing (key, value)
	{
		typeid(ircd::string_view), typeid(ircd::string_view)
	}
};

const ircd::database::descriptor
events_origin_server_ts_descriptor
{
	// name
	"origin_server_ts",

	// explanation
	R"(### protocol note:

	FEDERATION 4.1
	Timestamp in milliseconds on origin homeserver when this PDU was created.

	### developer note:
	key is event_id
	value is a machine integer (binary)

	TODO: consider unsigned rather than time_t because of millisecond precision

	)",

	// typing (key, value)
	{
		typeid(ircd::string_view), typeid(time_t)
	}
};

const ircd::database::descriptor
events_unsigned_descriptor
{
	// name
	"unsigned",

	// explanation
	R"(### protocol note:

	### developer note:
	key is event_id

	)",

	// typing (key, value)
	{
		typeid(ircd::string_view), typeid(ircd::string_view)
	}
};

const ircd::database::descriptor
events_signatures_descriptor
{
	// name
	"signatures",

	// explanation
	R"(### protocol note:

	### developer note:
	key is event_id

	)",

	// typing (key, value)
	{
		typeid(ircd::string_view), typeid(ircd::string_view)
	}
};

const ircd::database::descriptor
events_auth_events_descriptor
{
	// name
	"auth_events",

	// explanation
	R"(### protocol note:

	### developer note:
	key is event_id.
	)",

	// typing (key, value)
	{
		typeid(ircd::string_view), typeid(ircd::string_view)
	}
};

const ircd::database::descriptor
events_depth_descriptor
{
	// name
	"depth",

	// explanation
	R"(### protocol note:

	### developer note:
	key is event_id value is long integer
	)",

	// typing (key, value)
	{
		typeid(ircd::string_view), typeid(int64_t)
	}
};

const ircd::database::descriptor
events_hashes_descriptor
{
	// name
	"hashes",

	// explanation
	R"(### protocol note:

	### developer note:
	key is event_id.
	)",

	// typing (key, value)
	{
		typeid(ircd::string_view), typeid(ircd::string_view)
	}
};

const ircd::database::descriptor
events_membership_descriptor
{
	// name
	"membership",

	// explanation
	R"(### protocol note:

	### developer note:
	key is event_id.
	)",

	// typing (key, value)
	{
		typeid(ircd::string_view), typeid(ircd::string_view)
	}
};

const ircd::database::descriptor
events_prev_events_descriptor
{
	// name
	"prev_events",

	// explanation
	R"(### protocol note:

	### developer note:
	key is event_id.
	)",

	// typing (key, value)
	{
		typeid(ircd::string_view), typeid(ircd::string_view)
	}
};

const ircd::database::descriptor
events_prev_state_descriptor
{
	// name
	"prev_state",

	// explanation
	R"(### protocol note:

	### developer note:
	key is event_id.
	)",

	// typing (key, value)
	{
		typeid(ircd::string_view), typeid(ircd::string_view)
	}
};

/// prefix transform for event_id suffixes
///
/// This transform expects a concatenation ending with an event_id which means
/// the prefix can be the same for multiple event_id's; therefor we can find
/// or iterate "event_id in X" where X is some key like a room_id
///
const ircd::db::prefix_transform
event_id_in
{
	"event_id in",
	[](const ircd::string_view &key)
	{
		return key.find('$') != key.npos;
	},
	[](const ircd::string_view &key)
	{
		return rsplit(key, '$').first;
	}
};

const ircd::database::descriptor
event_id_in_sender
{
	// name
	"event_id in sender",

	// explanation
	R"(### developer note:

	key is "@sender$event_id"
	the prefix transform is in effect. this column indexes events by
	sender offering an iterable bound of the index prefixed by sender

	)",

	// typing (key, value)
	{
		typeid(ircd::string_view), typeid(ircd::string_view)
	},

	// options
	{},

	// comparator
	{},

	// prefix transform
	event_id_in,
};

const ircd::database::descriptor
state_head_for_event_id_in_room_id
{
	// name
	"state_head for event_id in room_id",

	// explanation
	R"(### developer note:

	key is "!room_id$event_id"
	the prefix transform is in effect. this column indexes events by
	room_id offering an iterable bound of the index prefixed by room_id

	)",

	// typing (key, value)
	{
		typeid(ircd::string_view), typeid(ircd::string_view)
	},

	// options
	{},

	// comparator - sorts from highest to lowest
	{}, //ircd::db::reverse_cmp_ircd::string_view{},

	// prefix transform
	event_id_in,
};

/// prefix transform for origin in
///
/// This transform expects a concatenation ending with an origin which means
/// the prefix can be the same for multiple origins; therefor we can find
/// or iterate "origin in X" where X is some repeated prefix
///
/// TODO: strings will have character conflicts. must address
const ircd::db::prefix_transform
origin_in
{
	"origin in",
	[](const ircd::string_view &key)
	{
		return has(key, ":::");
		//return key.find(':') != key.npos;
	},
	[](const ircd::string_view &key)
	{
		return split(key, ":::").first;
		//return rsplit(key, ':').first;
	}
};

const ircd::database::descriptor
origin_in_room_id
{
	// name
	"origin in room_id",

	// explanation
	R"(### developer note:

	key is "!room_id:origin"
	the prefix transform is in effect. this column indexes origins in a
	room_id offering an iterable bound of the index prefixed by room_id

	)",

	// typing (key, value)
	{
		typeid(ircd::string_view), typeid(ircd::string_view)
	},

	// options
	{},

	// comparator - sorts from highest to lowest
	{}, //ircd::db::reverse_cmp_string_view{},

	// prefix transform
	origin_in,
};

const ircd::database::descriptor
origin_joined_in_room_id
{
	// name
	"origin_joined in room_id",

	// explanation
	R"(### developer note:

	key is "!room_id:origin"
	the prefix transform is in effect. this column indexes origins in a
	room_id offering an iterable bound of the index prefixed by room_id

	)",

	// typing (key, value)
	{
		typeid(ircd::string_view), typeid(ircd::string_view)
	},

	// options
	{},

	// comparator - sorts from highest to lowest
	{}, //ircd::db::reverse_cmp_string_view{},

	// prefix transform
	origin_in,
};

/// prefix transform for room_id
///
/// This transform expects a concatenation ending with a room_id which means
/// the prefix can be the same for multiple room_id's; therefor we can find
/// or iterate "room_id in X" where X is some repeated prefix
///
const ircd::db::prefix_transform room_id_in
{
	"room_id in",
	[](const ircd::string_view &key)
	{
		return key.find('!') != key.npos;
	},
	[](const ircd::string_view &key)
	{
		return rsplit(key, '!').first;
	}
};

/// prefix transform for type,state_key in room_id
///
/// This transform is special for concatenating room_id with type and state_key
/// in that order with prefix being the room_id (this may change to room_id+
/// type
///
/// TODO: arbitrary type strings will have character conflicts. must address
/// TODO: with grammars.
const ircd::db::prefix_transform type_state_key_in_room_id
{
	"type,state_key in room_id",
	[](const ircd::string_view &key)
	{
		return key.find("..") != key.npos;
	},
	[](const ircd::string_view &key)
	{
		return split(key, "..").first;
	}
};

const ircd::database::descriptor
event_id_for_type_state_key_in_room_id
{
	// name
	"event_id for type,state_key in room_id",

	// explanation
	R"(### developer note:

	)",

	// typing (key, value)
	{
		typeid(ircd::string_view), typeid(ircd::string_view)
	},

	// options
	{},

	// comparator
	{},

	// prefix transform
	type_state_key_in_room_id
};

const ircd::database::descriptor
prev_event_id_for_event_id_in_room_id
{
	// name
	"prev_event_id for event_id in room_id",

	// explanation
	R"(### developer note:

	)",

	// typing (key, value)
	{
		typeid(ircd::string_view), typeid(ircd::string_view)
	},

	// options
	{},

	// comparator
	{},

	// prefix transform
	event_id_in
};

/// prefix transform for event_id in room_id,type,state_key
///
/// This transform is special for concatenating room_id with type and state_key
/// and event_id in that order with prefix being the room_id,type,state_key. This
/// will index multiple event_ids with the same type,state_key in a room which
/// allows for a temporal depth to the database; event_id for type,state_key only
/// resolves to a single latest event and overwrites itself as per the room state
/// algorithm whereas this can map all of them and then allows for tracing.
///
/// TODO: arbitrary type strings will have character conflicts. must address
/// TODO: with grammars.
const ircd::db::prefix_transform
event_id_in_room_id_type_state_key
{
	"event_id in room_id,type_state_key",
	[](const ircd::string_view &key)
	{
		return has(key, '$');
	},
	[](const ircd::string_view &key)
	{
		return split(key, '$').first;
	}
};

const ircd::database::descriptor
prev_event_id_for_type_state_key_event_id_in_room_id
{
	// name
	"prev_event_id for type,state_key,event_id in room_id",

	// explanation
	R"(### developer note:

	)",

	// typing (key, value)
	{
		typeid(ircd::string_view), typeid(ircd::string_view)
	},

	// options
	{},

	// comparator
	{},

	// prefix transform
	event_id_in_room_id_type_state_key
};

const ircd::database::descriptor
state_head
{
	// name
	"state_head",

	// explanation
	R"(### developer note:

	key is "!room_id"
	value is the key of a state_node

	)",

	// typing (key, value)
	{
		typeid(ircd::string_view), typeid(ircd::string_view)
	},

	// options
	{},

	// comparator
	{},

	// prefix transform
	{},
};

const ircd::database::descriptor
state_node
{
	// name
	"state_node",

	// explanation
	R"(### developer note:

	)",

	// typing (key, value)
	{
		typeid(ircd::string_view), typeid(ircd::string_view)
	},

	// options
	{},

	// comparator
	{},

	// prefix transform
	{},
};

decltype(ircd::m::event::description)
ircd::m::event::description
{
	{ "default" },

	////////
	//
	// These columns directly represent event fields indexed by event_id and
	// the value is the actual event values. Some values may be JSON, like
	// content.
	//
	events_event_id_descriptor,
	events_type_descriptor,
	events_content_descriptor,
	events_room_id_descriptor,
	events_sender_descriptor,
	events_state_key_descriptor,
	events_origin_descriptor,
	events_origin_server_ts_descriptor,
	events_unsigned_descriptor,
	events_signatures_descriptor,
	events_auth_events_descriptor,
	events_depth_descriptor,
	events_hashes_descriptor,
	events_membership_descriptor,
	events_prev_events_descriptor,
	events_prev_state_descriptor,

	////////
	//
	// These columns are metadata composed from the event data. Specifically,
	// they are designed for fast sequential iterations.
	//

	// (sender, event_id) => ()
	// Sequence of all events in all rooms for a sender, EVER
	// * broad but useful in cases
	event_id_in_sender,

	// (room_id, event_id) => (state_head)
	// Sequence of all events for a room, EVER
	// * broad but useful in cases
	// ? eliminate for prev_event?
	// ? eliminate/combine with state tree related?
	state_head_for_event_id_in_room_id,

	// (room_id, origin) => ()
	// Sequence of all origins for a room, EVER
	//TODO: value should have [JOIN, LEAVE, ...) counts/data
	//TODO: remove?
	origin_in_room_id,

	// (room_id, origin) => ()
	// Sequence of all origins with joined member for a room, AT PRESENT
	// * Intended to be a fast sequential iteration for sending out messages.
	origin_joined_in_room_id,

	// (room_id, type, state_key) => (event_id)
	// Sequence of events of type+state_key in a room, AT PRESENT
	// * Fast for current room state iteration, but only works for the present.
	event_id_for_type_state_key_in_room_id,

	////////
	//
	// These columns are metadata composed from the event data. They are
	// linked forward lists where the value is used to lookup the next key
	// TODO: these might be better as sequences; if not removed altogether.
	//

	// (room_id, event_id) => (prev event_id)
	// List of events in a room resolving to the previous event in a room
	// in our subjective euclidean tape TOTAL order.
	// * This is where any branches in the DAG are linearized based on how we
	// feel the state machine should execute them one by one.
	// * This is not a sequence; each value is the key for another lookup.
	prev_event_id_for_event_id_in_room_id,

	// (room_id, type, state_key, event_id) => (prev event_id)
	// Events of a (type, state_key) in a room resolving to the previous event
	// of (type, state_key) in a room in our subjective euclidean tape order.
	// * Similar to the above but focuses only on state events for various
	// "state chains"
	prev_event_id_for_type_state_key_event_id_in_room_id,

	////////
	//
	// These columns are metadata composed from the event data. They are
	// used to create structures that can represent the state of a room
	// at any given event.
	//

	// (room_id) => (state_head)
	state_head,

	// (state tree node id) => (state tree node)
	//
	state_node,
};
