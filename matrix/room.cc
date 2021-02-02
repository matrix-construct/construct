// Matrix Construct
//
// Copyright (C) Matrix Construct Developers, Authors & Contributors
// Copyright (C) 2016-2018 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

size_t
ircd::m::room::purge(const room &room)
{
	size_t ret(0);
	db::txn txn
	{
		*m::dbs::events
	};

	room.for_each([&txn, &ret]
	(const m::event::idx &event_idx)
	{
		const m::event::fetch event
		{
			std::nothrow, event_idx
		};

		if(!event.valid)
			return;

		m::dbs::write_opts opts;
		opts.op = db::op::DELETE;
		opts.event_idx = event_idx;
		m::dbs::write(txn, event, opts);
		++ret;
	});

	txn();
	return ret;
}

ircd::m::room
ircd::m::create(const id::room &room_id,
                const id::user &creator,
                const string_view &preset)
{
	return create(createroom
	{
		{ "room_id",  room_id },
		{ "creator",  creator },
		{ "preset",   preset  },
	});
}

ircd::m::event::id::buf
ircd::m::invite(const room &room,
                const id::user &target,
                const id::user &sender)
{
	json::iov content;
	return invite(room, target, sender, content);
}

ircd::m::event::id::buf
ircd::m::invite(const room &room,
                const user::id &target,
                const user::id &sender,
                json::iov &content)
{
	json::iov event;
	const json::iov::push push[]
	{
		{ event,    { "type",        "m.room.member"  }},
		{ event,    { "sender",      sender           }},
		{ event,    { "state_key",   target           }},
		{ content,  { "membership",  "invite"         }},
	};

	return commit(room, event, content);
}

ircd::m::event::id::buf
ircd::m::redact(const room &room,
                const id::user &sender,
                const id::event &event_id,
                const string_view &reason)
{
	json::iov event;
	const json::iov::push push[]
	{
		{ event,    { "type",       "m.room.redaction"  }},
		{ event,    { "sender",      sender             }},
		{ event,    { "redacts",     event_id           }},
	};

	json::iov content;
	const json::iov::set _reason
	{
		content, !empty(reason),
		{
			"reason", [&reason]() -> json::value
			{
				return reason;
			}
		}
	};

	return commit(room, event, content);
}

ircd::m::event::id::buf
ircd::m::notice(const room &room,
                const string_view &body)
{
	return message(room, me(), body, "m.notice");
}

ircd::m::event::id::buf
ircd::m::notice(const room &room,
                const m::id::user &sender,
                const string_view &body)
{
	return message(room, sender, body, "m.notice");
}

ircd::m::event::id::buf
ircd::m::msghtml(const room &room,
                 const m::id::user &sender,
                 const string_view &html,
                 const string_view &alt,
                 const string_view &msgtype)
{
	return message(room, sender,
	{
		{ "msgtype",         msgtype                       },
		{ "format",          "org.matrix.custom.html"      },
		{ "body",            { alt?: html, json::STRING }  },
		{ "formatted_body",  { html, json::STRING }        },
	});
}

ircd::m::event::id::buf
ircd::m::message(const room &room,
                 const m::id::user &sender,
                 const string_view &body,
                 const string_view &msgtype)
{
	return message(room, sender,
	{
		{ "body",     { body,    json::STRING } },
		{ "msgtype",  { msgtype, json::STRING } },
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
ircd::m::annotate(const room &room,
                  const id::user &sender,
                  const id::event &target,
                  const string_view &key)
{
	json::iov relates;
	const json::iov::push push
	{
		relates, { "key", key },
	};

	return react(room, sender, target, "m.annotation", relates);
}

ircd::m::event::id::buf
ircd::m::react(const room &room,
               const id::user &sender,
               const id::event &target,
               const string_view &rel_type,
               json::iov &relates)
{
	json::iov content;
	thread_local char buf[event::MAX_SIZE];
	const json::iov::push push[]
	{
		{ relates,  { "event_id",      target                                  }},
		{ relates,  { "rel_type",      rel_type                                }},
		{ content,  { "m.relates_to",  stringify(mutable_buffer{buf}, relates) }},
	};

	return send(room, sender, "m.reaction", content);
}

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wstack-usage="
ircd::m::event::id::buf
__attribute__((stack_protect))
ircd::m::send(const room &room,
              const m::id::user &sender,
              const string_view &type,
              const string_view &state_key,
              const json::members &contents)
{
	const size_t contents_count
	{
		std::min(contents.size(), json::object::max_sorted_members)
	};

	json::iov _content;
	json::iov::push content[contents_count]; // 48B each
	return send(room, sender, type, state_key, make_iov(_content, content, contents_count, contents));
}
#pragma GCC diagnostic pop

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wstack-usage="
ircd::m::event::id::buf
__attribute__((stack_protect))
ircd::m::send(const room &room,
              const m::id::user &sender,
              const string_view &type,
              const string_view &state_key,
              const json::object &contents)
{
	const size_t contents_count
	{
		std::min(contents.size(), json::object::max_sorted_members)
	};

	json::iov _content;
	json::iov::push content[contents_count]; // 48B each
	return send(room, sender, type, state_key, make_iov(_content, content, contents_count, contents));
}
#pragma GCC diagnostic pop

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

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wstack-usage="
ircd::m::event::id::buf
__attribute__((stack_protect))
ircd::m::send(const room &room,
              const m::id::user &sender,
              const string_view &type,
              const json::members &contents)
{
	const size_t contents_count
	{
		std::min(contents.size(), json::object::max_sorted_members)
	};

	json::iov _content;
	json::iov::push content[contents_count]; // 48B each
	return send(room, sender, type, make_iov(_content, content, contents_count, contents));
}
#pragma GCC diagnostic pop

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wstack-usage="
ircd::m::event::id::buf
__attribute__((stack_protect))
ircd::m::send(const room &room,
              const m::id::user &sender,
              const string_view &type,
              const json::object &contents)
{
	const size_t contents_count
	{
		std::min(contents.size(), json::object::max_sorted_members)
	};

	json::iov _content;
	json::iov::push content[contents_count]; // 48B each
	return send(room, sender, type, make_iov(_content, content, contents_count, contents));
}
#pragma GCC diagnostic pop

ircd::m::event::id::buf
ircd::m::send(const room &room,
              const id::user &sender,
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
	// Set the room_id on the iov
	json::iov::push room_id
	{
		event, { "room_id", room.room_id }
	};

	vm::copts opts
	{
		room.copts?
			*room.copts:
			vm::default_copts
	};

	// Some functionality on this server may create an event on behalf
	// of remote users. It's safe for us to mask this here, but eval'ing
	// this event in any replay later will require special casing.
	opts.non_conform.set(event::conforms::MISMATCH_ORIGIN_SENDER);

	// Don't need this here
	opts.phase.reset(vm::phase::VERIFY);

	return vm::eval
	{
		event, contents, opts
	};
}

ircd::m::id::room::buf
ircd::m::room_id(const id::room_alias &room_alias)
{
	char buf[m::id::MAX_SIZE + 1];
	static_assert(sizeof(buf) <= 256);
	return room_id(buf, room_alias);
}

ircd::m::id::room::buf
ircd::m::room_id(const id::event &event_id)
{
	char buf[m::id::MAX_SIZE + 1];
	static_assert(sizeof(buf) <= 256);
	return room_id(buf, event_id);
}

ircd::m::id::room::buf
ircd::m::room_id(const string_view &mxid)
{
	char buf[m::id::MAX_SIZE + 1];
	static_assert(sizeof(buf) <= 256);
	return room_id(buf, mxid);
}

ircd::m::id::room::buf
ircd::m::room_id(const event::idx &event_idx)
{
	char buf[m::id::MAX_SIZE + 1];
	static_assert(sizeof(buf) <= 256);
	return room_id(buf, event_idx);
}

ircd::m::id::room
ircd::m::room_id(const mutable_buffer &out,
                 const string_view &mxid)
{
	switch(m::sigil(mxid))
	{
		case id::ROOM:
			return id::room{out, mxid};

		case id::USER:
		{
			const m::user::room user_room(mxid);
			return string_view
			{
				data(out), copy(out, user_room.room_id)
			};
		}

		case id::NODE:
		{
			const m::node node(lstrip(mxid, ':'));
			const m::node::room node_room(node);
			return string_view
			{
				data(out), copy(out, node_room.room_id)
			};
		}

		case id::EVENT:
			return room_id(out, id::event{mxid});

		default:
			return room_id(out, id::room_alias{mxid});
	}
}

ircd::m::id::room
ircd::m::room_id(const mutable_buffer &out,
                 const event::idx &event_idx)
try
{
	room::id ret;
	m::get(event_idx, "room_id", [&out, &ret]
	(const room::id &room_id)
	{
		ret = string_view { data(out), copy(out, room_id) };
	});

	return ret;
}
catch(const m::NOT_FOUND &e)
{
	throw m::NOT_FOUND
	{
		"resolving room_id from event_idx :%s",
		e.what(),
	};
}

ircd::m::id::room
ircd::m::room_id(const mutable_buffer &out,
                 const id::event &event_id)
try
{
	room::id ret;
	m::get(event_id, "room_id", [&out, &ret]
	(const room::id &room_id)
	{
		ret = string_view { data(out), copy(out, room_id) };
	});

	return ret;
}
catch(const m::NOT_FOUND &e)
{
	throw m::NOT_FOUND
	{
		"resolving room_id from event_id :%s",
		e.what(),
	};
}

ircd::m::id::room
ircd::m::room_id(const mutable_buffer &out,
                 const id::room_alias &room_alias)
try
{
	room::id ret;
	room::aliases::cache::get(room_alias, [&out, &ret]
	(const room::id &room_id)
	{
		ret = string_view { data(out), copy(out, room_id) };
	});

	return ret;
}
catch(const m::NOT_FOUND &e)
{
	throw m::NOT_FOUND
	{
		"resolving room_id from alias :%s",
		e.what(),
	};
}

bool
ircd::m::exists(const id::room_alias &room_alias,
                const bool &remote_query)
{
	if(room::aliases::cache::has(room_alias))
		return true;

	if(!remote_query)
		return false;

	return room::aliases::cache::get(std::nothrow, room_alias, [](const room::id &room_id) {});
}

int64_t
ircd::m::depth(const id::room &room_id)
{
	return std::get<int64_t>(top(room_id));
}

int64_t
ircd::m::depth(std::nothrow_t,
               const id::room &room_id)
{
	const auto it
	{
		dbs::room_events.begin(room_id)
	};

	if(!it)
		return -1;

	const auto part
	{
		dbs::room_events_key(it->first)
	};

	return std::get<0>(part);
}

ircd::m::event::idx
ircd::m::head_idx(const id::room &room_id)
{
	return std::get<event::idx>(top(room_id));
}

ircd::m::event::idx
ircd::m::head_idx(std::nothrow_t,
                  const id::room &room_id)
{
	const auto it
	{
		dbs::room_events.begin(room_id)
	};

	if(!it)
		return 0;

	const auto part
	{
		dbs::room_events_key(it->first)
	};

	return std::get<1>(part);
}

ircd::m::event::id::buf
ircd::m::head(const id::room &room_id)
{
	return std::get<event::id::buf>(top(room_id));
}

ircd::m::event::id::buf
ircd::m::head(std::nothrow_t,
              const id::room &room_id)
{
	return std::get<event::id::buf>(top(std::nothrow, room_id));
}

std::tuple<ircd::m::event::id::buf, int64_t, ircd::m::event::idx>
ircd::m::top(const id::room &room_id)
{
	const auto ret
	{
		top(std::nothrow, room_id)
	};

	if(std::get<int64_t>(ret) == -1)
		throw m::NOT_FOUND
		{
			"No head for room %s", string_view{room_id}
		};

	return ret;
}

std::tuple<ircd::m::event::id::buf, int64_t, ircd::m::event::idx>
ircd::m::top(std::nothrow_t,
             const id::room &room_id)
{
	const auto it
	{
		dbs::room_events.begin(room_id)
	};

	if(!it)
		return
		{
			event::id::buf{}, -1, 0
		};

	const auto part
	{
		dbs::room_events_key(it->first)
	};

	const int64_t &depth
	{
		int64_t(std::get<0>(part))
	};

	const event::idx &event_idx
	{
		std::get<1>(part)
	};

	std::tuple<event::id::buf, int64_t, event::idx> ret
	{
		event::id::buf{}, depth, event_idx
	};

	m::event_id(std::nothrow, event_idx, [&ret]
	(const event::id &event_id)
	{
		std::get<event::id::buf>(ret) = event_id;
	});

	return ret;
}

ircd::m::id::user::buf
ircd::m::any_user(const room &room,
                  const string_view &host,
                  const string_view &membership)
{
	user::id::buf ret;
	const room::members members{room};
	members.for_each(membership, [&host, &ret]
	(const auto &user_id, const auto &event_idx)
	{
		if(host && user_id.host() != host)
			return true;

		ret = user_id;
		return false;
	});

	return ret;
}

/// Receive the join_rule of the room into buffer of sufficient size.
/// The protocol does not specify a join_rule string longer than 7
/// characters but do be considerate of the future. This function
/// properly defaults the string as per the protocol spec.
ircd::string_view
ircd::m::join_rule(const mutable_buffer &out,
                   const room &room)
{
	static const string_view default_join_rule
	{
		"invite"
	};

	string_view ret
	{
		default_join_rule
	};

	const event::keys::include keys
	{
		"content"
	};

	const m::event::fetch::opts fopts
	{
		keys, room.fopts? room.fopts->gopts : db::gopts{}
	};

	const room::state state
	{
		room, &fopts
	};

	state.get(std::nothrow, "m.room.join_rules", "", [&ret, &out]
	(const m::event &event)
	{
		const auto &content
		{
			json::get<"content"_>(event)
		};

		const json::string &rule
		{
			content.get("join_rule", default_join_rule)
		};

		ret = string_view
		{
			data(out), copy(out, rule)
		};
	});

	return ret;
}

ircd::string_view
ircd::m::version(const mutable_buffer &buf,
                 const room &room)
{
	const auto event_idx
	{
		room.get("m.room.create", "")
	};

	string_view ret;
	m::get(event_idx, "content", [&buf, &ret]
	(const json::object &content)
	{
		const json::string &version
		{
			content.get("room_version", "1"_sv)
		};

		ret = strlcpy
		{
			buf, version
		};
	});

	if(unlikely(!ret))
		throw m::NOT_FOUND
		{
			"Failed to find room %s to query its version",
			string_view{room.room_id}
		};

	return ret;
}

ircd::string_view
ircd::m::version(const mutable_buffer &buf,
                 const room &room,
                 std::nothrow_t)
{
	const auto event_idx
	{
		room.get(std::nothrow, "m.room.create", "")
	};

	string_view ret
	{
		strlcpy{buf, "1"_sv}
	};

	m::get(std::nothrow, event_idx, "content", [&buf, &ret]
	(const json::object &content)
	{
		const json::string &version
		{
			content.get("room_version", "1"_sv)
		};

		ret = strlcpy
		{
			buf, version
		};
	});

	return ret;
}

ircd::string_view
ircd::m::type(const mutable_buffer &buf,
              const room &room)
{
	string_view ret;
	const auto event_idx
	{
		room.get(std::nothrow, "m.room.create", "")
	};

	m::get(std::nothrow, event_idx, "content", [&buf, &ret]
	(const json::object &content)
	{
		const json::string &type
		{
			content.get("type")
		};

		ret = strlcpy
		{
			buf, type
		};
	});

	return ret;
}

bool
ircd::m::contains(const id::room &room_id,
                  const event::idx &event_idx)
{
	return m::query(event_idx, "room_id", [&room_id]
	(const string_view &_room_id) -> bool
	{
		return _room_id == room_id;
	});
}

ircd::m::id::user::buf
ircd::m::creator(const id::room &room_id)
{
	// Query the sender field of the event to get the creator. This is for
	// future compatibility if the content.creator field gets eliminated.
	static const event::fetch::opts fopts
	{
		event::keys::include {"sender"}
	};

	const room::state state
	{
		room_id, &fopts
	};

	id::user::buf ret;
	state.get("m.room.create", "", [&ret]
	(const m::event &event)
	{
		ret = user::id
		{
			json::get<"sender"_>(event)
		};
	});

	return ret;
}

//
// boolean suite
//

/// The only members are from our origin, in any membership state. This
/// indicates we won't have any other federation servers that could possibly
/// be party to anything about this room.
bool
ircd::m::local_only(const room &room)
{
	// Branch to test if any remote users are joined to the room, meaning
	// this result must be false; this is a fast query.
	if(remote_joined(room))
		return false;

	const room::members members
	{
		room
	};

	return members.for_each([]
	(const id::user &user_id)
	{
		return my(user_id);
	});
}

/// Member(s) from our server are presently joined to the room. Returns false
/// if there's a room on the server where all of our users have left. Note that
/// some internal rooms have no memberships at all and this will also be false.
/// This can return true if other servers have memberships in the room too, as
/// long as one of our users is joined.
bool
ircd::m::local_joined(const room &room)
{
	const room::members members
	{
		room
	};

	return !for_each([&members]
	(const homeserver &homeserver)
	{
		// return false to break and return false; true to continue
		return members.empty("join", origin(homeserver));
	});
}

/// Member(s) from another server are presently joined to the room. For example
/// if another user leaves a PM with our user who is still joined, this returns
/// false. This can return true even if the room has no memberships in any
/// state from our server, as long as there's a joined member from a remote.
bool
ircd::m::remote_joined(const room &room)
{
	const room::members members
	{
		room
	};

	return !members.for_each("join", []
	(const id::user &user_id)
	{
		return my(user_id)? true : false; // false to break.
	});
}

bool
ircd::m::visible(const room &room,
                 const string_view &mxid,
                 const event *const &event)
{
	if(event)
		return m::visible(*event, mxid);

	const m::event event_
	{
		json::members
		{
			{ "event_id",  room.event_id  },
			{ "room_id",   room.room_id   },
		}
	};

	return m::visible(event_, mxid);
}

/// Test of the join_rule of the room is the argument.
bool
ircd::m::join_rule(const room &room,
                   const string_view &rule)
{
	char buf[32];
	return join_rule(buf, room) == rule;
}

bool
ircd::m::creator(const room::id &room_id,
                 const user::id &user_id)
{
	const auto creator_user_id
	{
		creator(room_id)
	};

	return creator_user_id == user_id;
}

bool
ircd::m::federated(const id::room &room_id)
{
	static const m::event::fetch::opts fopts
	{
		event::keys::include { "content" },
	};

	const m::room::state state
	{
		room_id, &fopts
	};

	bool ret;
	state.get("m.room.create", "", [&ret]
	(const m::event &event)
	{
		ret = json::get<"content"_>(event).get("m.federate", true);
	});

	return ret;
}

/// Determine if this is an internal room. The following must be satisfied:
///
/// - The room was created by this origin.
/// - The creator was the server itself, not any other user.
bool
ircd::m::internal(const id::room &room_id)
{
	const m::room room
	{
		room_id
	};

	if(!my(room))
		return false;

	const room::state state
	{
		room
	};

	const auto create_idx
	{
		state.get(std::nothrow, "m.room.create", "")
	};

	return m::query(std::nothrow, create_idx, "sender", []
	(const string_view &sender)
	{
		return sender == me();
	});
}

bool
ircd::m::exists(const id::room &room_id)
{
	const m::room::events it
	{
		room_id, 0UL
	};

	if(!it)
		return false;

	if(likely(it.depth() < 2UL))
		return true;

	if(my_host(room_id.host()) && creator(room_id, me()))
		return true;

	return false;
}

bool
ircd::m::exists(const room &room)
{
	return exists(room.room_id);
}

//
// util
//

bool
ircd::m::operator==(const room &a, const room &b)
{
	return !(a != b);
}

bool
ircd::m::operator!=(const room &a_, const room &b_)
{
	const string_view &a{a_.room_id}, &b{b_.room_id};
	return a != b;
}

bool
ircd::m::operator!(const room &a)
{
	return !a.room_id;
}

bool
ircd::m::my(const room &room)
{
	return my(room.room_id);
}

//
// room
//

/// A room index is just the event::idx of its create event.
ircd::m::event::idx
ircd::m::room::index(const room::id &room_id)
{
	const auto ret
	{
		index(room_id, std::nothrow)
	};

	if(!ret)
		throw m::NOT_FOUND
		{
			"No index for room %s", string_view{room_id}
		};

	return ret;
}

ircd::m::event::idx
ircd::m::room::index(const room::id &room_id,
                     std::nothrow_t)
{
	uint64_t depth{0};
	room::events it
	{
		room_id, depth
	};

	return it? it.event_idx() : 0;
}

//
// room::room
//

size_t
ircd::m::room::count()
const
{
	size_t ret(0);
	for_each(event::closure_idx_bool{[&ret]
	(const event::idx &event_idx)
	{
		++ret;
		return true;
	}});

	return ret;
}

size_t
ircd::m::room::count(const string_view &type)
const
{
	size_t ret(0);
	for_each(type, event::closure_idx_bool{[&ret]
	(const event::idx &event_idx)
	{
		++ret;
		return true;
	}});

	return ret;
}

size_t
ircd::m::room::count(const string_view &type,
                     const string_view &state_key)
const
{
	size_t ret(0);
	for_each(type, event::closure_idx_bool{[&state_key, &ret]
	(const event::idx &event_idx)
	{
		ret += query(std::nothrow, event_idx, "state_key", [&state_key]
		(const string_view &_state_key) -> bool
		{
			return state_key == _state_key;
		});

		return true;
	}});

	return ret;
}

bool
ircd::m::room::has(const string_view &type)
const
{
	return get(std::nothrow, type, event::closure{});
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
	bool ret{false};
	for_each(type, event::closure_bool{[&ret, &closure]
	(const event &event)
	{
		if(closure)
			closure(event);

		ret = true;
		return false;
	}});

	return ret;
}

ircd::m::event::idx
ircd::m::room::get(const string_view &type)
const
{
	const event::idx ret
	{
		get(std::nothrow, type)
	};

	if(unlikely(!ret))
		throw m::NOT_FOUND
		{
			"No events of type '%s' found in '%s'",
			type,
			room_id
		};

	return ret;
}

ircd::m::event::idx
ircd::m::room::get(std::nothrow_t,
                   const string_view &type)
const
{
	event::idx ret{0};
	for_each(type, event::closure_idx_bool{[&ret]
	(const event::idx &event_idx)
	{
		ret = event_idx;
		return false;
	}});

	return ret;
}

ircd::m::event::idx
ircd::m::room::get(const string_view &type,
                   const string_view &state_key)
const
{
	return state(*this).get(type, state_key);
}

ircd::m::event::idx
ircd::m::room::get(std::nothrow_t,
                   const string_view &type,
                   const string_view &state_key)
const
{
	return state(*this).get(std::nothrow, type, state_key);
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

void
ircd::m::room::for_each(const event::closure &closure)
const
{
	for_each(string_view{}, closure);
}

bool
ircd::m::room::for_each(const event::closure_bool &closure)
const
{
	return for_each(string_view{}, closure);
}

void
ircd::m::room::for_each(const event::id::closure &closure)
const
{
	for_each(string_view{}, closure);
}

bool
ircd::m::room::for_each(const event::id::closure_bool &closure)
const
{
	return for_each(string_view{}, closure);
}

void
ircd::m::room::for_each(const event::closure_idx &closure)
const
{
	for_each(string_view{}, closure);
}

bool
ircd::m::room::for_each(const event::closure_idx_bool &closure)
const
{
	return for_each(string_view{}, closure);
}

void
ircd::m::room::for_each(const string_view &type,
                        const event::closure &closure)
const
{
	for_each(type, event::closure_bool{[&closure]
	(const event &event)
	{
		closure(event);
		return true;
	}});
}

bool
ircd::m::room::for_each(const string_view &type,
                        const event::closure_bool &closure)
const
{
	event::fetch event
	{
		fopts? *fopts : event::fetch::default_opts
	};

	return for_each(type, event::closure_idx_bool{[&closure, &event]
	(const event::idx &event_idx)
	{
		if(!seek(std::nothrow, event, event_idx))
			return true;

		return closure(event);
	}});
}

void
ircd::m::room::for_each(const string_view &type,
                        const event::id::closure &closure)
const
{
	for_each(type, event::id::closure_bool{[&closure]
	(const event::id &event_id)
	{
		closure(event_id);
		return true;
	}});
}

bool
ircd::m::room::for_each(const string_view &type,
                        const event::id::closure_bool &closure)
const
{
	return for_each(type, event::closure_idx_bool{[&closure]
	(const event::idx &idx)
	{
		bool ret{true};
		m::event_id(std::nothrow, idx, [&ret, &closure]
		(const event::id &event_id)
		{
			ret = closure(event_id);
		});

		return ret;
	}});
}

void
ircd::m::room::for_each(const string_view &type,
                        const event::closure_idx &closure)
const
{
	for_each(type, event::closure_idx_bool{[&closure]
	(const event::idx &idx)
	{
		closure(idx);
		return true;
	}});
}

bool
ircd::m::room::for_each(const string_view &type,
                        const event::closure_idx_bool &closure)
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

	events it{*this};
	for(; it; --it)
	{
		const auto &event_idx
		{
			it.event_idx()
		};

		bool match
		{
			empty(type) // allow empty type to always match and bypass query
		};

		if(!match)
			column(byte_view<string_view>(event_idx), std::nothrow, [&match, &type]
			(const string_view &value)
			{
				match = value == type;
			});

		if(match)
			if(!closure(event_idx))
				return false;
	}

	return true;
}
