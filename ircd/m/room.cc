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

uint64_t
ircd::m::depth(const id::room &room_id)
{
	uint64_t depth;
	head(room_id, depth);
	return depth;
}

int64_t
ircd::m::depth(std::nothrow_t,
               const id::room &room_id)
{
	int64_t depth;
	head(std::nothrow, room_id, depth);
	return depth;
}

ircd::m::id::event::buf
ircd::m::head(const id::room &room_id)
{
	uint64_t depth;
	return head(room_id, depth);
}

ircd::m::id::event::buf
ircd::m::head(std::nothrow_t,
              const id::room &room_id)
{
	int64_t depth;
	return head(std::nothrow, room_id, depth);
}

ircd::m::id::event::buf
ircd::m::head(const id::room &room_id,
              uint64_t &depth)
{
	const auto ret
	{
		head(std::nothrow, room_id, reinterpret_cast<int64_t &>(depth))
	};

	if(depth == uint64_t(-1))
		throw m::NOT_FOUND{};

	return ret;
}

ircd::m::id::event::buf
ircd::m::head(std::nothrow_t,
              const id::room &room_id,
              int64_t &depth)
{
	const auto it
	{
		dbs::room_events.begin(room_id)
	};

	if(!it)
	{
		depth = -1;
		return {};
	}

	const auto &key{it->first};
	const auto part
	{
		dbs::room_events_key(key)
	};

	depth = std::get<0>(part);
	return std::get<1>(part);
}

bool
ircd::m::exists(const id::room &room_id)
{
	const auto it
	{
		dbs::room_events.begin(room_id)
	};

	return bool(it);
}

bool
ircd::m::my(const room &room)
{
	return my(room.room_id);
}

//
// room
//

bool
ircd::m::room::membership(const m::id::user &user_id,
                          const string_view &membership)
const
{
	char buf[64];
	return this->membership(buf, user_id) == membership;
}

ircd::string_view
ircd::m::room::membership(const mutable_buffer &out,
                          const m::id::user &user_id)
const
{
	string_view ret;
	const state state{*this};
	state.get(std::nothrow, "m.room.member"_sv, user_id, [&out, &ret]
	(const m::event &event)
	{
		assert(json::get<"type"_>(event) == "m.room.member");
		ret = { data(out), copy(out, unquote(at<"membership"_>(event))) };
	});

	return ret;
}

bool
ircd::m::room::has(const string_view &type)
const
{
	return get(std::nothrow, type, nullptr);
}

void
ircd::m::room::get(const string_view &type,
                   const event::closure &closure)
const
{
	if(!get(std::nothrow, type, closure))
		throw m::NOT_FOUND
		{
			"No events of type '%s' found in '%s'",
			type,
			room_id
		};
}

bool
ircd::m::room::get(std::nothrow_t,
                   const string_view &type,
                   const event::closure &closure)
const
{
	static constexpr auto idx
	{
		json::indexof<event, "type"_>()
	};

	auto &column
	{
		dbs::event_column.at(idx)
	};

	bool ret{false};
	messages it{*this};
	for(; it; --it)
	{
		const auto &event_id{it.event_id()};
		column(event_id, [&ret, &type](const string_view &value)
		{
			ret = value == type;
		});

		if(ret)
			break;
	}

	if(ret && closure)
		closure(*it);

	return ret;
}

void
ircd::m::room::get(const string_view &type,
                   const string_view &state_key,
                   const event::closure &closure)
const
{
	const state state{*this};
	state.get(type, state_key, closure);
}

bool
ircd::m::room::get(std::nothrow_t,
                   const string_view &type,
                   const string_view &state_key,
                   const event::closure &closure)
const
{
	const state state{*this};
	return state.get(std::nothrow, type, state_key, closure);
}

bool
ircd::m::room::has(const string_view &type,
                   const string_view &state_key)
const
{
	const state state{*this};
	return state.has(type, state_key);
}

//
// room::messages
//

ircd::m::room::messages::messages(const m::room &room)
:room{room}
{
	seek();
}

ircd::m::room::messages::messages(const m::room &room,
                                  const event::id &event_id)
:room{room}
{
	seek(event_id);
}

ircd::m::room::messages::messages(const m::room &room,
                                  const uint64_t &depth)
:room{room}
{
	seek(depth);
}

const ircd::m::event &
ircd::m::room::messages::operator*()
{
	return fetch(std::nothrow);
};

bool
ircd::m::room::messages::seek()
{
	this->it = dbs::room_events.begin(room.room_id);
	return bool(*this);
}

bool
ircd::m::room::messages::seek(const event::id &event_id)
{
	static constexpr auto idx
	{
		json::indexof<event, "depth"_>()
	};

	auto &column
	{
		dbs::event_column.at(idx)
	};

	uint64_t depth;
	column(event_id, [&](const string_view &binary)
	{
		assert(size(binary) == sizeof(depth));
		depth = byte_view<uint64_t>(binary);
	});

	char buf[m::state::KEY_MAX_SZ];
	const auto seek_key
	{
		dbs::room_events_key(buf, room.room_id, depth, event_id)
	};

	this->it = dbs::room_events.begin(seek_key);
	return bool(*this);
}

bool
ircd::m::room::messages::seek(const uint64_t &depth)
{
	char buf[m::state::KEY_MAX_SZ];
	const auto seek_key
	{
		dbs::room_events_key(buf, room.room_id, depth)
	};

	this->it = dbs::room_events.begin(seek_key);
	return bool(*this);
}

const ircd::m::event::id &
ircd::m::room::messages::event_id()
{
	assert(bool(*this));
	const auto &key{it->first};
	const auto part{dbs::room_events_key(key)};
	_event_id = std::get<1>(part);
	return _event_id;
}

const ircd::m::event &
ircd::m::room::messages::fetch()
{
	m::seek(_event, event_id());
	return _event;
}

const ircd::m::event &
ircd::m::room::messages::fetch(std::nothrow_t)
{
	if(!m::seek(_event, event_id(), std::nothrow))
		_event = m::event::fetch{};

	return _event;
}

//
// room::state
//

ircd::m::room::state::state(const m::room &room)
:room_id{room.room_id}
,event_id{room.event_id? room.event_id : head(room_id)}
{
	refresh();
}

const ircd::m::state::id &
ircd::m::room::state::refresh()
{
	this->root_id = dbs::state_root(root_id_buf, room_id, event_id);
	return this->root_id;
}

void
ircd::m::room::state::get(const string_view &type,
                          const string_view &state_key,
                          const event::closure &closure)
const
{
	get(type, state_key, event::id::closure{[&closure]
	(const event::id &event_id)
	{
		event::fetch event
		{
			event_id
		};

		if(event.valid(event_id))
			closure(event);
	}});
}

void
ircd::m::room::state::get(const string_view &type,
                          const string_view &state_key,
                          const event::id::closure &closure)
const
{
	m::state::get(root_id, type, state_key, [&closure]
	(const string_view &event_id)
	{
		closure(unquote(event_id));
	});
}

bool
ircd::m::room::state::get(std::nothrow_t,
                          const string_view &type,
                          const string_view &state_key,
                          const event::closure &closure)
const
{
	return get(std::nothrow, type, state_key, event::id::closure{[&closure]
	(const event::id &event_id)
	{
		event::fetch event
		{
			event_id, std::nothrow
		};

		if(event.valid(event_id))
			closure(event);
	}});
}

bool
ircd::m::room::state::get(std::nothrow_t,
                          const string_view &type,
                          const string_view &state_key,
                          const event::id::closure &closure)
const
{
	return m::state::get(std::nothrow, root_id, type, state_key, [&closure]
	(const string_view &event_id)
	{
		closure(unquote(event_id));
	});
}

bool
ircd::m::room::state::has(const string_view &type)
const
{
	return test(type, event::id::closure_bool{[](const m::event::id &)
	{
		return true;
	}});
}

bool
ircd::m::room::state::has(const string_view &type,
                          const string_view &state_key)
const
{
	return m::state::get(std::nothrow, root_id, type, state_key, []
	(const string_view &event_id)
	{
	});
}

size_t
ircd::m::room::state::count()
const
{
	return m::state::count(root_id);
}

size_t
ircd::m::room::state::count(const string_view &type)
const
{
	return m::state::count(root_id, type);
}

bool
ircd::m::room::state::test(const event::closure_bool &closure)
const
{
	event::fetch event;
	return test(event::id::closure_bool{[&event, &closure]
	(const event::id &event_id)
	{
		if(seek(event, unquote(event_id), std::nothrow))
			if(closure(event))
				return true;

		return false;
	}});
}

bool
ircd::m::room::state::test(const event::id::closure_bool &closure)
const
{
	return m::state::test(root_id, [&closure]
	(const json::array &key, const string_view &event_id)
	{
		return closure(unquote(event_id));
	});
}

bool
ircd::m::room::state::test(const string_view &type,
                           const event::closure_bool &closure)
const
{
	event::fetch event;
	return test(type, event::id::closure_bool{[&event, &closure]
	(const event::id &event_id)
	{
		if(seek(event, unquote(event_id), std::nothrow))
			if(closure(event))
				return true;

		return false;
	}});
}

bool
ircd::m::room::state::test(const string_view &type,
                           const event::id::closure_bool &closure)
const
{
	return m::state::test(root_id, type, [&closure]
	(const json::array &key, const string_view &event_id)
	{
		return closure(unquote(event_id));
	});
}

bool
ircd::m::room::state::test(const string_view &type,
                           const string_view &state_key_lb,
                           const event::closure_bool &closure)
const
{
	event::fetch event;
	return test(type, state_key_lb, event::id::closure_bool{[&event, &closure]
	(const event::id &event_id)
	{
		if(seek(event, unquote(event_id), std::nothrow))
			if(closure(event))
				return true;

		return false;
	}});
}

bool
ircd::m::room::state::test(const string_view &type,
                           const string_view &state_key_lb,
                           const event::id::closure_bool &closure)
const
{
	return m::state::test(root_id, type, state_key_lb, [&closure]
	(const json::array &key, const string_view &event_id)
	{
		return closure(unquote(event_id));
	});
}

void
ircd::m::room::state::for_each(const event::closure &closure)
const
{
	event::fetch event;
	for_each(event::id::closure{[&event, &closure]
	(const event::id &event_id)
	{
		if(seek(event, event_id, std::nothrow))
			closure(event);
	}});
}

void
ircd::m::room::state::for_each(const event::id::closure &closure)
const
{
	m::state::for_each(root_id, [&closure]
	(const json::array &key, const string_view &event_id)
	{
		closure(unquote(event_id));
	});
}

void
ircd::m::room::state::for_each(const string_view &type,
                               const event::closure &closure)
const
{
	event::fetch event;
	for_each(type, event::id::closure{[&event, &closure]
	(const event::id &event_id)
	{
		if(seek(event, event_id, std::nothrow))
			closure(event);
	}});
}

void
ircd::m::room::state::for_each(const string_view &type,
                               const event::id::closure &closure)
const
{
	m::state::for_each(root_id, type, [&closure]
	(const json::array &key, const string_view &event_id)
	{
		closure(unquote(event_id));
	});
}

//
// room::members
//

void
ircd::m::room::members::for_each(const event::closure &view)
const
{
	test([&view](const m::event &event)
	{
		view(event);
		return false;
	});
}

void
ircd::m::room::members::for_each(const string_view &membership,
                                 const event::closure &view)
const
{
	test(membership, [&view](const m::event &event)
	{
		view(event);
		return false;
	});
}

bool
ircd::m::room::members::test(const event::closure_bool &view)
const
{
	const room::state state
	{
		room
	};

	static const string_view type{"m.room.member"};
	return state.test(type, event::closure_bool{[&view]
	(const m::event &event)
	{
		return view(event);
	}});
}

bool
ircd::m::room::members::test(const string_view &membership,
                             const event::closure_bool &view)
const
{
	//TODO: optimize with sequential column strat
	return test([&membership, &view](const event &event)
	{
		if(at<"membership"_>(event) == membership)
			if(view(event))
				return true;

		return false;
	});
}

//
// room::origins
//

void
ircd::m::room::origins::for_each(const closure &view)
const
{
	test([&view](const string_view &origin)
	{
		view(origin);
		return false;
	});
}

bool
ircd::m::room::origins::test(const closure_bool &view)
const
{
	db::index &index
	{
		dbs::room_origins
	};

	auto it
	{
		index.begin(room.room_id)
	};

	string_view last;
	char lastbuf[256];
	for(; bool(it); ++it)
	{
		const string_view &key{it->first};
		const string_view &origin_member //TODO: XXX
		{
			split(key, ":::").second
		};

		const string_view &origin //TODO: XXX
		{
			split(origin_member, "@").first
		};

		if(origin == last)
			continue;

		if(view(origin))
			return true;

		last = { lastbuf, copy(lastbuf, origin) };
	}

	return false;
}

//
// room::state::tuple
//

ircd::m::room::state::tuple::tuple(const m::room &room,
                                   const mutable_buffer &buf)
{

}

ircd::m::room::state::tuple::tuple(const json::array &pdus)
{
	for(const json::object &pdu : pdus)
	{
		const m::event &event{pdu};
		json::set(*this, at<"type"_>(event), event);
	}
}

std::string
ircd::m::pretty(const room::state::tuple &state)
{
	std::string ret;
	std::stringstream s;
	pubsetbuf(s, ret, 2048);

	const auto out{[&s]
	(const string_view &key, const auto &event)
	{
		if(!json::get<"event_id"_>(event))
			return;

		s << std::setw(28) << std::right << key
		  << " : " << at<"event_id"_>(event)
		  << " " << json::get<"sender"_>(event)
		  << " " << json::get<"depth"_>(event)
		  << " " << pretty_oneline(event::prev{event})
		  << std::endl;
	}};

	json::for_each(state, out);
	resizebuf(s, ret);
	return ret;
}

std::string
ircd::m::pretty_oneline(const room::state::tuple &state)
{
	std::string ret;
	std::stringstream s;
	pubsetbuf(s, ret, 1024);

	const auto out{[&s]
	(const string_view &key, const auto &event)
	{
		if(!json::get<"event_id"_>(event))
			return;

		s << key << " ";
	}};

	json::for_each(state, out);
	resizebuf(s, ret);
	return ret;
}
