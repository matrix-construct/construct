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
		"redacts",
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
	out("redacts", json::get<"redacts"_>(event));

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

bool
ircd::m::my(const event &event)
{
	return my(event::id(at<"event_id"_>(event)));
}

bool
ircd::m::my(const id::event &event_id)
{
	return self::host(event_id.host());
}

//
// event
//

ircd::m::event::event(const id &id,
                      const mutable_buffer &buf)
{
	assert(bool(dbs::events));

	db::gopts opts;
	opts.snapshot = database::snapshot{*dbs::events};
	for(size_t i(0); i < dbs::event_column.size(); ++i)
	{
		const db::cell cell
		{
			dbs::event_column[i], id, opts
		};

		db::assign(*this, cell, id);
	}

	const json::object obj
	{
		string_view{data(buf), json::print(buf, *this)}
	};

	new (this) m::event(obj);
}

//
// event::prev
//

ircd::m::event::id
ircd::m::event::prev::auth_event(const uint &idx)
const
{
	return std::get<0>(auth_events(idx));
}

ircd::m::event::id
ircd::m::event::prev::prev_state(const uint &idx)
const
{
	return std::get<0>(prev_states(idx));
}

ircd::m::event::id
ircd::m::event::prev::prev_event(const uint &idx)
const
{
	return std::get<0>(prev_events(idx));
}

std::tuple<ircd::m::event::id, ircd::string_view>
ircd::m::event::prev::auth_events(const uint &idx)
const
{
	const json::array &auth_event
	{
		at<"auth_events"_>(*this).at(idx)
	};

	return
	{
		unquote(auth_event.at(0)), unquote(auth_event[1])
	};
}

std::tuple<ircd::m::event::id, ircd::string_view>
ircd::m::event::prev::prev_states(const uint &idx)
const
{
	const json::array &state_event
	{
		at<"prev_state"_>(*this).at(idx)
	};

	return
	{
		unquote(state_event.at(0)), unquote(state_event[1])
	};
}

std::tuple<ircd::m::event::id, ircd::string_view>
ircd::m::event::prev::prev_events(const uint &idx)
const
{
	const json::array &prev_event
	{
		at<"prev_events"_>(*this).at(idx)
	};

	return
	{
		unquote(prev_event.at(0)), unquote(prev_event[1])
	};
}

//
// event::fetch
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
	db::seek(fetch.row, string_view{event_id});
	if(!fetch.row.valid(event_id))
		return false;

	auto &event{static_cast<m::event &>(fetch)};
	assign(event, fetch.row, event_id);
	return true;
}

// db::row finds the layout of an event tuple because we pass this as a
// reference argument to its constructor, rather than making db::row into
// a template type.
const ircd::m::event
_dummy_event_;

/// Seekless constructor.
ircd::m::event::fetch::fetch()
:row
{
	*dbs::events, string_view{}, _dummy_event_, cell
}
{
}

/// Seek to event_id and populate this event from database.
/// Throws if event not in database.
ircd::m::event::fetch::fetch(const event::id &event_id)
:row
{
	*dbs::events, event_id, _dummy_event_, cell
}
{
	if(!row.valid(event_id))
		throw m::NOT_FOUND
		{
			"%s not found in database", event_id
		};

	assign(*this, row, event_id);
}

/// Seek to event_id and populate this event from database.
/// Event is not populated if not found in database.
ircd::m::event::fetch::fetch(const event::id &event_id,
                             std::nothrow_t)
:row
{
	*dbs::events, event_id, _dummy_event_, cell
}
{
	if(row.valid(event_id))
		assign(*this, row, event_id);
}

bool
ircd::m::event::fetch::valid(const event::id &event_id)
const
{
	return row.valid(event_id);
}

//
// event::conforms
//

namespace ircd::m
{
	const size_t event_conforms_num{num_of<event::conforms::code>()};
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
	"USELESS_REDACTS_ID",
	"MISSING_MEMBERSHIP",
	"INVALID_MEMBERSHIP",
	"USELESS_MEMBERSHIP",
	"MISSING_CONTENT_MEMBERSHIP",
	"INVALID_CONTENT_MEMBERSHIP",
	"MISSING_PREV_EVENTS",
	"MISSING_PREV_STATE",
	"DEPTH_NEGATIVE",
	"DEPTH_ZERO",
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

ircd::m::event::conforms::conforms(const event &e,
                                   const uint64_t &skip)
:conforms{e}
{
	report &= ~skip;
}

ircd::m::event::conforms::conforms(const event &e)
:report{0}
{
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
	if(false)
		set(INVALID_ORIGIN);

	if(json::get<"type"_>(e) == "m.room.redaction")
		if(!valid(m::id::EVENT, json::get<"redacts"_>(e)))
			set(INVALID_OR_MISSING_REDACTS_ID);

	if(json::get<"type"_>(e) != "m.room.redaction")
		if(!empty(json::get<"redacts"_>(e)))
			set(USELESS_REDACTS_ID);

	if(json::get<"type"_>(e) == "m.room.member")
		if(empty(json::get<"membership"_>(e)))
			set(MISSING_MEMBERSHIP);

	if(json::get<"type"_>(e) == "m.room.member")
		if(!all_of<std::islower>(json::get<"membership"_>(e)))
			set(INVALID_MEMBERSHIP);

	if(json::get<"type"_>(e) != "m.room.member")
		if(!empty(json::get<"membership"_>(e)))
			set(USELESS_MEMBERSHIP);

	if(json::get<"type"_>(e) == "m.room.member")
		if(empty(unquote(json::get<"content"_>(e).get("membership"))))
			set(MISSING_CONTENT_MEMBERSHIP);

	if(json::get<"type"_>(e) == "m.room.member")
		if(!all_of<std::islower>(unquote(json::get<"content"_>(e).get("membership"))))
			set(INVALID_CONTENT_MEMBERSHIP);

	if(json::get<"type"_>(e) != "m.room.create")
		if(empty(json::get<"prev_events"_>(e)))
			set(MISSING_PREV_EVENTS);

	if(json::get<"type"_>(e) != "m.room.create")
		if(!empty(json::get<"state_key"_>(e)))
			if(empty(json::get<"prev_state"_>(e)))
				set(MISSING_PREV_STATE);

	if(json::get<"depth"_>(e) < 0)
		set(DEPTH_NEGATIVE);

	if(json::get<"type"_>(e) != "m.room.create")
		if(json::get<"depth"_>(e) == 0)
			set(DEPTH_ZERO);
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

		consume(buf, copy(buf, reflect(code(i))));
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
