// Matrix Construct
//
// Copyright (C) Matrix Construct Developers, Authors & Contributors
// Copyright (C) 2016-2019 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

namespace ircd::m::rooms::summary
{
	static void chunk_remote(const room &, json::stack::object &o);
	static void chunk_local(const room &, json::stack::object &o);

	extern hookfn<vm::eval &> create_public_room;
}

/// Create the public rooms room during initial database bootstrap.
/// This hooks the creation of the !ircd room which is a fundamental
/// event indicating the database has just been created.
decltype(ircd::m::rooms::summary::create_public_room)
ircd::m::rooms::summary::create_public_room
{
	{
		{ "_site",       "vm.effect"      },
		{ "room_id",     "!ircd"          },
		{ "type",        "m.room.create"  },
	},
	[](const m::event &event, m::vm::eval &)
	{
		auto &my
		{
			m::my(at<"origin"_>(event))
		};

		const m::room::id::buf public_room_id
		{
			"public", origin(my)
		};

		m::create(public_room_id, me());
	}
};

//
// rooms::summary::fetch
//

decltype(ircd::m::rooms::summary::fetch::timeout)
ircd::m::rooms::summary::fetch::timeout
{
	{ "name",     "ircd.m.rooms.fetch.timeout" },
	{ "default",  45L /*  matrix.org :-/    */ },
};

decltype(ircd::m::rooms::summary::fetch::limit)
ircd::m::rooms::summary::fetch::limit
{
	{ "name",     "ircd.m.rooms.fetch.limit" },
	{ "default",  64L                        },
};

//
// fetch::fetch
//

ircd::m::rooms::summary::fetch::fetch(const string_view &origin,
                                      const string_view &since,
                                      const size_t &limit,
                                      const string_view &search_term)
{
	m::fed::public_rooms::opts opts;
	opts.limit = limit;
	opts.since = since;
	opts.include_all_networks = true;
	opts.search_term = search_term;
	const unique_buffer<mutable_buffer> buf
	{
		// Buffer for headers and send content; received content is dynamic
		16_KiB
	};

	m::fed::public_rooms request
	{
		origin, buf, std::move(opts)
	};

	const auto code
	{
		request.get(seconds(timeout))
	};

	const json::object response
	{
		request
	};

	const json::array &chunk
	{
		response.get("chunk")
	};

	for(const json::object summary : chunk)
	{
		const json::string &room_id
		{
			summary.at("room_id")
		};

		summary::set(room_id, origin, summary);
	}

	this->total_room_count_estimate =
	{
		response.get("total_room_count_estimate", 0UL)
	};

	this->next_batch = json::string
	{
		response.get("next_batch", string_view{})
	};
}

//
// rooms::summary
//

void
ircd::m::rooms::summary::del(const m::room &room)
{
	for_each(room, [&room]
	(const string_view &origin, const event::idx &event_idx)
	{
		del(room, origin);
		return true;
	});
}

ircd::m::event::id::buf
ircd::m::rooms::summary::del(const m::room &room,
                             const string_view &origin)
{
	const m::room::id::buf public_room_id
	{
		"public", my_host()
	};

	const m::room::state state
	{
		public_room_id
	};

	char state_key_buf[m::event::STATE_KEY_MAX_SIZE];
	const auto state_key
	{
		make_state_key(state_key_buf, room.room_id, origin)
	};

	const m::event::idx &event_idx
	{
		state.get(std::nothrow, "ircd.rooms.summary", state_key)
	};

	if(!event_idx)
		return {};

	const m::event::id::buf event_id
	{
		m::event_id(event_idx)
	};

	return redact(public_room_id, me(), event_id, "delisted");
}

ircd::m::event::id::buf
ircd::m::rooms::summary::set(const m::room &room)
{
	if(!exists(room))
		throw m::NOT_FOUND
		{
			"Cannot set a summary for room '%s' which I have no state for",
			string_view{room.room_id}
		};

	const unique_buffer<mutable_buffer> buf
	{
		48_KiB
	};

	const json::object summary
	{
		get(buf, room)
	};

	return set(room.room_id, my_host(), summary);
}

ircd::m::event::id::buf
ircd::m::rooms::summary::set(const m::room::id &room_id,
                             const string_view &origin,
                             const json::object &summary)
{
	const m::room::id::buf public_room_id
	{
		"public", my_host()
	};


	char state_key_buf[event::STATE_KEY_MAX_SIZE];
	const auto state_key
	{
		make_state_key(state_key_buf, room_id, origin)
	};

	return send(public_room_id, me(), "ircd.rooms.summary", state_key, summary);
}

ircd::json::object
ircd::m::rooms::summary::get(const mutable_buffer &buf,
                             const m::room &room)
{
	json::stack out{buf};
	{
		json::stack::object obj{out};
		get(obj, room);
	}

	return json::object
	{
		out.completed()
	};
}

void
ircd::m::rooms::summary::get(json::stack::object &obj,
                             const m::room &room)
{
	return exists(room)?
		chunk_local(room, obj):
		chunk_remote(room, obj);
}

bool
ircd::m::rooms::summary::has(const room::id &room_id,
                             const string_view &origin)
{
	return !for_each(room_id, [&origin]
	(const string_view &_origin, const json::object &summary)
	{
		if(!origin)
			return false;

		if(origin && _origin == origin)
			return false;

		return true;
	});
}

bool
ircd::m::rooms::summary::for_each(const room::id &room_id,
                                  const closure &closure)
{
	return for_each(room_id, [&closure]
	(const string_view &origin, const event::idx &event_idx)
	{
		bool ret{true};
		m::get(std::nothrow, event_idx, "content", [&closure, &origin, &ret]
		(const json::object &content)
		{
			ret = closure(origin, content);
		});

		return ret;
	});
}

bool
ircd::m::rooms::summary::for_each(const room::id &room_id,
                                  const closure_idx &closure)
{
	const m::room::id::buf public_room_id
	{
		"public", my_host()
	};

	const m::room::state state
	{
		public_room_id
	};

	char state_key_buf[m::event::STATE_KEY_MAX_SIZE];
	const auto state_key
	{
		make_state_key(state_key_buf, room_id, string_view{})
	};

	bool ret{true};
	state.for_each("ircd.rooms.summary", state_key, [&room_id, &closure, &ret]
	(const auto &type, const auto &state_key, const auto &event_idx)
	{
		const auto &[_room_id, origin]
		{
			unmake_state_key(state_key)
		};

		if(_room_id != room_id)
			return false;

		if(!closure(origin, event_idx))
			ret = false;

		return ret;
	});

	return ret;
}

std::pair<ircd::m::room::id, ircd::string_view>
ircd::m::rooms::summary::unmake_state_key(const string_view &key)
{
	return rsplit(key, '!');
}

ircd::string_view
ircd::m::rooms::summary::make_state_key(const mutable_buffer &buf,
                                        const m::room::id &room_id,
                                        const string_view &origin)
{
	return fmt::sprintf
	{
		buf, "%s!%s",
		string_view{room_id},
		origin,
	};
}

//
// internal
//

void
ircd::m::rooms::summary::chunk_remote(const m::room &room,
                                      json::stack::object &obj)
{
	for_each(room, [&obj]
	(const string_view &origin, const json::object &summary)
	{
		obj.append(summary);
		return false;
	});
}

void
ircd::m::rooms::summary::chunk_local(const m::room &room,
                                     json::stack::object &obj)
{
	const m::room::state state
	{
		room
	};

	// Closure over boilerplate primary room state queries; i.e matrix
	// m.room.$type events with state key "" where the content is directly
	// presented to the closure.
	const auto query{[&state]
	(const auto &type, const auto &content_key, const auto &closure)
	{
		const auto event_idx
		{
			state.get(std::nothrow, type, "")
		};

		m::get(std::nothrow, event_idx, "content", [&content_key, &closure]
		(const json::object &content)
		{
			const json::string &value
			{
				content.get(content_key)
			};

			closure(value);
		});
	}};

	// Aliases array
	{
		json::stack::member aliases_m{obj, "aliases"};
		json::stack::array array{aliases_m};
		state.for_each("m.room.aliases", [&array]
		(const m::event &event)
		{
			const json::object &content
			{
				json::get<"content"_>(event)
			};

			const json::array aliases
			{
				content["aliases"]
			};

			for(const json::string a : aliases)
				array.append(a);
		});
	}

	query("m.room.avatar_url", "url", [&obj]
	(const string_view &value)
	{
		json::stack::member
		{
			obj, "avatar_url", value
		};
	});

	query("m.room.canonical_alias", "alias", [&obj]
	(const string_view &value)
	{
		json::stack::member
		{
			obj, "canonical_alias", value
		};
	});

	query("m.room.guest_access", "guest_access", [&obj]
	(const string_view &value)
	{
		json::stack::member
		{
			obj, "guest_can_join", json::value
			{
				value == "can_join"
			}
		};
	});

	query("m.room.name", "name", [&obj]
	(const string_view &value)
	{
		json::stack::member
		{
			obj, "name", value
		};
	});

	// num_join_members
	{
		const m::room::members members{room};
		json::stack::member
		{
			obj, "num_joined_members", json::value
			{
				long(members.count("join"))
			}
		};
	}

	// room_id
	{
		json::stack::member
		{
			obj, "room_id", room.room_id
		};
	}

	query("m.room.topic", "topic", [&obj]
	(const string_view &value)
	{
		json::stack::member
		{
			obj, "topic", value
		};
	});

	query("m.room.history_visibility", "history_visibility", [&obj]
	(const string_view &value)
	{
		json::stack::member
		{
			obj, "world_readable", json::value
			{
				value == "world_readable"
			}
		};
	});
}
