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

const ircd::m::room::id::buf
init_room_id
{
	"init", ircd::my_host()
};

ircd::m::room
ircd::m::create(const id::room &room_id,
                const id::user &creator,
                const string_view &type)
{
	return create(room_id, creator, init_room_id, type);
}

ircd::m::room
ircd::m::create(const id::room &room_id,
                const id::user &creator,
                const id::room &parent,
                const string_view &type)
{
	json::iov event;
	json::iov content;
	const json::iov::push push[]
	{
		{ event,     { "sender",   creator  }},
		{ content,   { "creator",  creator  }},
	};

	const json::iov::add_if _parent
	{
		content, !parent.empty() && parent.local() != "init",
		{
			"parent", parent
		}
	};

	const json::iov::add_if _type
	{
		content, !type.empty() && type != "room",
		{
			"type", type
		}
	};

	json::iov::set _set[]
	{
		{ event,  { "depth",       1                }},
		{ event,  { "type",        "m.room.create"  }},
		{ event,  { "state_key",   ""               }},
	};

	room room
	{
		room_id
	};

	commit(room, event, content);
	return room;
}

ircd::m::event::id::buf
ircd::m::join(const room &room,
              const m::id::user &user_id)
{
	return membership(room, user_id, "join");
}

ircd::m::event::id::buf
ircd::m::leave(const room &room,
               const m::id::user &user_id)
{
	return membership(room, user_id, "leave");
}

ircd::m::event::id::buf
ircd::m::membership(const room &room,
                    const m::id::user &user_id,
                    const string_view &membership)
{
	json::iov event;
	json::iov content;
	json::iov::push push[]
	{
		{ event,    { "type",       "m.room.member"  }},
		{ event,    { "sender",      user_id         }},
		{ event,    { "state_key",   user_id         }},
		{ event,    { "membership",  membership      }},
		{ content,  { "membership",  membership      }},
	};

/*
	if(this->membership(user_id, membership))
		throw m::ALREADY_MEMBER
		{
			"Member '%s' is already '%s'.", string_view{user_id}, membership
		};
*/
	return commit(room, event, content);
}

ircd::m::event::id::buf
ircd::m::message(const room &room,
                 const m::id::user &sender,
                 const string_view &body,
                 const string_view &msgtype)
{
	return message(room, sender,
	{
		{ "body",      body     },
		{ "msgtype",   msgtype  }
	});
}

ircd::m::event::id::buf
ircd::m::message(const room &room,
                 const m::id::user &sender,
                 const json::members &contents)
{
	return send(room, sender, "m.room.message", contents);
}

ircd::m::event::id::buf
ircd::m::send(const room &room,
              const m::id::user &sender,
              const string_view &type,
              const string_view &state_key,
              const json::members &contents)
{
	json::iov::push content[contents.size()];
	return send(room, sender, type, state_key, make_iov(content, contents.size(), contents));
}

ircd::m::event::id::buf
ircd::m::send(const room &room,
              const m::id::user &sender,
              const string_view &type,
              const string_view &state_key,
              const json::object &contents)
{
	json::iov::push content[contents.size()];
	return send(room, sender, type, state_key, make_iov(content, contents.size(), contents));
}

ircd::m::event::id::buf
ircd::m::send(const room &room,
              const m::id::user &sender,
              const string_view &type,
              const string_view &state_key,
              const json::iov &content)
{
	json::iov event;
	const json::iov::push push[]
	{
		{ event,    { "sender",     sender     }},
		{ event,    { "type",       type       }},
		{ event,    { "state_key",  state_key  }},
	};

	return commit(room, event, content);
}

ircd::m::event::id::buf
ircd::m::send(const room &room,
              const m::id::user &sender,
              const string_view &type,
              const json::members &contents)
{
	json::iov::push content[contents.size()];
	return send(room, sender, type, make_iov(content, contents.size(), contents));
}

ircd::m::event::id::buf
ircd::m::send(const room &room,
              const m::id::user &sender,
              const string_view &type,
              const json::object &contents)
{
	json::iov::push content[contents.count()];
	return send(room, sender, type, make_iov(content, contents.count(), contents));
}

ircd::m::event::id::buf
ircd::m::send(const room &room,
              const m::id::user &sender,
              const string_view &type,
              const json::iov &content)
{
	json::iov event;
	const json::iov::push push[]
	{
		{ event,    { "sender",  sender  }},
		{ event,    { "type",    type    }},
	};

	return commit(room, event, content);
}

ircd::m::event::id::buf
ircd::m::commit(const room &room,
                json::iov &event,
                const json::iov &contents)
{
	const json::iov::push room_id
	{
		event, { "room_id", room.room_id }
	};

	int64_t depth;
	const auto prev_event_id
	{
		head(room.room_id, depth)
	};

	//TODO: LCOCK
	const json::iov::set_if depth_
	{
		event, !event.has("depth"),
		{
			"depth", depth + 1
		}
	};

	const string_view auth_events{};
	const string_view prev_state{};

	json::value prev_event0[]
	{
		prev_event_id
	};

	json::value prev_events[]
	{
		{ prev_event0, 1 }
	};

	//TODO: LOLCK
	const json::iov::push prevs[]
	{
		{ event, { "auth_events",  auth_events  }},
		{ event, { "prev_state",   prev_state   }},
		{ event, { "prev_events",  { prev_events, 1 } } },
	};

	return m::vm::commit(event, contents);
}

int64_t
ircd::m::depth(const id::room &room_id)
{
	int64_t depth;
	head(room_id, depth);
	return depth;
}

ircd::m::id::event::buf
ircd::m::head(const id::room &room_id)
{
	int64_t depth;
	return head(room_id, depth);
}

ircd::m::id::event::buf
ircd::m::head(const id::room &room_id,
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

ircd::m::room::room(const alias &alias,
                    const event::id &event_id)
:room_id{}
,event_id{event_id}
{
	assert(0); //TODO: translate
}

bool
ircd::m::room::membership(const m::id::user &user_id,
                          const string_view &membership)
const
{
	const event::id::buf event_id_buf
	{
		!event_id? head(room_id) : string_view{}
	};

	const event::id event_id
	{
		this->event_id? this->event_id : event_id_buf
	};

	m::state::id_buffer state_root_buf;
	const auto state_root
	{
		dbs::state_root(state_root_buf, room_id, event_id)
	};

	bool ret{false};
	static const auto type{"m.room.member"};
	m::state::get(std::nothrow, state_root, type, user_id, [&membership, &ret]
	(const string_view &event_id)
	{
		event::fetch event;
		if(!seek(event, event_id, std::nothrow))
			return;

		assert(json::get<"type"_>(event) == "m.room.member");
		ret = at<"membership"_>(event) == membership;
	});

	return ret;
}

bool
ircd::m::room::prev(const event::closure &closure)
const try
{
	auto it
	{
		dbs::room_events.begin(room_id)
	};

	if(!it)
		return false;

	const auto &key{it->first};
	const auto part
	{
		dbs::room_events_key(key)
	};

	const auto event_id
	{
		std::get<1>(part)
	};

	const event::fetch event
	{
		event_id
	};

	closure(event);
	return true;
}
catch(const NOT_FOUND &)
{
	return false;
}

bool
ircd::m::room::get(const event::closure &closure)
const
{
	auto it
	{
		dbs::room_events.begin(room_id)
	};

	event::fetch event;
	if(it) do
	{
		const auto &key{it->first};
		const auto part
		{
			dbs::room_events_key(key)
		};

		const auto event_id
		{
			std::get<1>(part)
		};

		if(seek(event, event_id, std::nothrow))
			closure(event);
	}
	while(++it); else return false;

	return true;
}

bool
ircd::m::room::get(const string_view &type,
                   const event::closure &closure)
const
{
	return get(type, string_view{}, closure);
}

bool
ircd::m::room::get(const string_view &type,
                   const string_view &state_key,
                   const event::closure &closure)
const
{
	const event::id::buf event_id_buf
	{
		!event_id? head(room_id) : string_view{}
	};

	const event::id event_id
	{
		this->event_id? this->event_id : event_id_buf
	};

	m::state::id_buffer state_root_buf;
	const auto state_root
	{
		dbs::state_root(state_root_buf, room_id, event_id)
	};

	return m::state::get(std::nothrow, state_root, type, state_key, [&closure]
	(const string_view &event_id)
	{
		event::fetch event;
		if(seek(event, event_id, std::nothrow))
			closure(event);
	});
}

bool
ircd::m::room::has(const string_view &type)
const
{
	return test(type, [](const auto &event)
	{
		return true;
	});
}

bool
ircd::m::room::has(const string_view &type,
                   const string_view &state_key)
const
{
	const event::id::buf event_id_buf
	{
		!event_id? head(room_id) : string_view{}
	};

	const event::id event_id
	{
		this->event_id? this->event_id : event_id_buf
	};

	m::state::id_buffer state_root_buf;
	const auto state_root
	{
		dbs::state_root(state_root_buf, room_id, event_id)
	};

	return m::state::get(std::nothrow, state_root, type, state_key, []
	(const string_view &event_id)
	{
	});
}

void
ircd::m::room::for_each(const string_view &type,
                        const event::closure &closure)
const
{
	const vm::query<vm::where::equal> query
	{
		{ "room_id",      room_id           },
		{ "type",         type              },
	};

	return m::vm::for_each(query, closure);
}

bool
ircd::m::room::test(const string_view &type,
                    const event::closure_bool &closure)
const
{
	const vm::query<vm::where::equal> query
	{
		{ "room_id",      room_id           },
		{ "type",         type              },
	};

	return m::vm::test(query, closure);
}

//
// room::members
//

bool
ircd::m::room::members::until(const event::closure_bool &view)
const
{
	const event::id::buf event_id_buf
	{
		!room.event_id? head(room.room_id) : string_view{}
	};

	const event::id event_id
	{
		room.event_id? room.event_id : event_id_buf
	};

	m::state::id_buffer state_root_buf;
	const auto state_root
	{
		dbs::state_root(state_root_buf, room.room_id, event_id)
	};

	event::fetch event;
	return m::state::each(state_root, "m.room.member", [&event, &view]
	(const json::array &key, const string_view &val)
	{
		const string_view event_id
		{
			unquote(val)
		};

		if(!seek(event, event_id, std::nothrow))
			return false;

		return view(event);
	});
}

bool
ircd::m::room::members::until(const string_view &membership,
                              const event::closure_bool &view)
const
{
	//TODO: optimize with sequential column strat
	return until([&membership, &view](const event &event)
	{
		if(at<"membership"_>(event) == membership)
			if(!view(event))
				return false;

		return true;
	});
}

//
// room::members::origins
//

bool
ircd::m::room::members::origins::until(const closure_bool &view)
const
{
	db::index index
	{
		*event::events, "origin in room_id"
	};

	auto it(index.begin(room.room_id));
	for(; bool(it); ++it)
	{
		const string_view &key(it->first);
		const string_view &origin(split(key, ":::").second); //TODO: XXX
		if(!view(origin))
			return false;
	}

	return true;
}

//
// room::state
//

ircd::m::room::state::state(const room::id &room_id,
                            const event::id &event_id,
                            const mutable_buffer &buf)
{
	fetch tab
	{
		event_id, room_id, buf
	};

	new (this) state{tab};
}

ircd::m::room::state::state(fetch &tab)
{
	io::acquire(tab);

	if(bool(tab.error))
		std::rethrow_exception(tab.error);

	new (this) state{tab.pdus};
}

ircd::m::room::state::state(const json::array &pdus)
{
	for(const json::object &pdu : pdus)
	{
		const m::event &event{pdu};
		json::set(*this, at<"type"_>(event), event);
	}
}

std::string
ircd::m::pretty(const room::state &state)
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
ircd::m::pretty_oneline(const room::state &state)
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
