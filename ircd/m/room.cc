/*
 * Copyright (C) 2016 Charybdis Development Team
 * Copyright (C) 2016 Jason Volk <jason@zemos.net>
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice is present in all copies.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

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

	room.send(event, content);
	return room;
}

ircd::m::event::id::buf
ircd::m::join(m::room room,
              const m::id::user &user_id)
{
	return membership(room, user_id, "join");
}

ircd::m::event::id::buf
ircd::m::leave(m::room room,
               const m::id::user &user_id)
{
	return membership(room, user_id, "leave");
}

ircd::m::event::id::buf
ircd::m::membership(m::room room,
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
	return room.send(event, content);
}

ircd::m::event::id::buf
ircd::m::message(m::room room,
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
ircd::m::message(m::room room,
                 const m::id::user &sender,
                 const json::members &contents)
{
	return send(room, sender, "m.room.message", contents);
}

ircd::m::event::id::buf
ircd::m::send(m::room room,
              const m::id::user &sender,
              const string_view &type,
              const string_view &state_key,
              const json::members &contents)
{
	size_t i(0);
	json::iov content;
	json::iov::push _content[contents.size()];
	for(const auto &member : contents)
		new (_content + i++) json::iov::push(content, member);

	return send(room, sender, type, state_key, content);
}

ircd::m::event::id::buf
ircd::m::send(m::room room,
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

	return room.send(event, content);
}

ircd::m::event::id::buf
ircd::m::send(m::room room,
              const m::id::user &sender,
              const string_view &type,
              const json::members &contents)
{
	size_t i(0);
	json::iov content;
	json::iov::push _content[contents.size()];
	for(const auto &member : contents)
		new (_content + i++) json::iov::push(content, member);

	return send(room, sender, type, content);
}

ircd::m::event::id::buf
ircd::m::send(m::room room,
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

	return room.send(event, content);
}

bool
ircd::m::exists(const id::room &room_id)
{
	const vm::query<vm::where::equal> query
	{
		{ "room_id", room_id },
	};

	return m::vm::test(query);
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

ircd::m::event::id::buf
ircd::m::room::message(json::iov &event,
                       const json::iov &content)
{
	const json::iov::set _type[]
	{
		{ event,  { "type", "m.room.message" }},
	};

	const json::strung c //TODO: child iov
	{
		content
	};

	const json::iov::set_if _content[]
	{
		{ event, !content.empty(), { "content", string_view{c} }},
	};

	return send(event);
}

ircd::m::event::id::buf
ircd::m::room::send(json::iov &event,
                    const json::iov &content)
{
	const json::strung c //TODO: child iov
	{
		content
	};

	const json::iov::set_if _content[]
	{
		{ event, !content.empty(), { "content", string_view{c} }},
	};

	return send(event);
}

ircd::m::event::id::buf
ircd::m::room::send(json::iov &event)
{
	const json::iov::set room_id
	{
		event, { "room_id", this->room_id }
	};

	//std::cout << this->room_id << " at " << this->maxdepth() << std::endl;

	// TODO: XXX
	// commitment to room here @ exclusive acquisition of depth

	const json::iov::defaults depth
	{
		event, { "depth", int64_t(this->maxdepth()) + 1 }
	};

	return m::vm::commit(event);
}

bool
ircd::m::room::membership(const m::id::user &user_id,
                          const string_view &membership)
const
{
	const vm::query<vm::where::equal> member_event
	{
		{ "room_id",      room_id           },
		{ "type",        "m.room.member"    },
		{ "state_key",    user_id           },
	};

	if(!membership)
		return m::vm::test(member_event);

	const vm::query<vm::where::test> membership_test{[&membership]
	(const auto &event)
	{
		const json::object &content
		{
			json::at<"content"_>(event)
		};

		const auto &existing_membership
		{
			unquote(content.at("membership"))
		};

		return membership == existing_membership;
	}};

	return m::vm::test(member_event && membership_test);
}

bool
ircd::m::room::prev(const event::closure &closure)
const
{
	const vm::query<vm::where::equal> query
	{
		{ "room_id", room_id },
	};

	vm::cursor cursor
	{
		"prev_event_id for event_id in room_id", &query
	};

	auto it(cursor.begin(room_id));
	if(!it)
		return false;

	closure(*it);
	return true;
}

bool
ircd::m::room::get(const event::closure &closure)
const
{
	const vm::query<vm::where::equal> query
	{
		{ "room_id", room_id },
	};

	vm::cursor cursor
	{
		"event_id in room_id", &query
	};

	auto it(cursor.begin(room_id));
	if(!it)
		return false;

	closure(*it);
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
	const vm::query<vm::where::equal> query
	{
		{ "room_id",      room_id           },
		{ "type",         type              },
		{ "state_key",    state_key         },
	};

	return m::vm::test(query, [&closure]
	(const auto &event)
	{
		closure(event);
		return true;
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
	const vm::query<vm::where::equal> query
	{
		{ "room_id",      room_id           },
		{ "type",         type              },
		{ "state_key",    state_key         },
	};

	return m::vm::test(query);
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

/// academic search
uint64_t
ircd::m::room::maxdepth()
const
{
	event::id::buf buf;
	return maxdepth(buf);
}

/// academic search
uint64_t
ircd::m::room::maxdepth(event::id::buf &buf)
const
{
	const vm::query<vm::where::equal> query
	{
		{ "room_id", room_id },
	};

	int64_t depth{0};
	vm::for_each(query, [&buf, &depth]
	(const auto &event)
	{
		if(json::get<"depth"_>(event) > depth)
		{
			depth = json::get<"depth"_>(event);
			buf = json::get<"event_id"_>(event);
		}
	});

	return depth;
}

//
// room::members
//

bool
ircd::m::room::members::until(const event::closure_bool &view)
const
{
	const vm::query<vm::where::equal> query
	{
		{ "room_id",      room.room_id      },
		{ "type",         "m.room.member"   },
	};

	return m::vm::until(query, [&view]
	(const auto &event)
	{
		return view(event);
	});
}

bool
ircd::m::room::members::until(const string_view &membership,
                              const event::closure_bool &view)
const
{
	const vm::query<vm::where::equal> query
	{
		{ "room_id",      room.room_id      },
		{ "type",         "m.room.member"   },
		{ "membership",   membership        },
	};

	return m::vm::until(query, [&view]
	(const auto &event)
	{
		return view(event);
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
