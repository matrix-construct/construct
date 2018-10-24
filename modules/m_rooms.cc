// Matrix Construct
//
// Copyright (C) Matrix Construct Developers, Authors & Contributors
// Copyright (C) 2016-2018 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

namespace ircd::m::rooms
{
	extern "C" void _summary_chunk(const m::room &room, json::stack::object &obj);
	extern "C" size_t _count_public(const string_view &server);
	extern "C" bool _for_each_public(const string_view &room_id_lb, const room::id::closure_bool &);
	extern "C" bool _for_each(const string_view &room_id_lb, const room::id::closure_bool &);
	static void create_public_room(const m::event &, m::vm::eval &);

	extern const room::id::buf public_room_id;
	extern m::hookfn<vm::eval &> create_public_room_hook;
}

ircd::mapi::header
IRCD_MODULE
{
	"Matrix rooms interface; modular components"
};

decltype(ircd::m::rooms::public_room_id)
ircd::m::rooms::public_room_id
{
    "public", ircd::my_host()
};

/// Create the public rooms room during initial database bootstrap.
/// This hooks the creation of the !ircd room which is a fundamental
/// event indicating the database has just been created.
decltype(ircd::m::rooms::create_public_room_hook)
ircd::m::rooms::create_public_room_hook
{
	create_public_room,
	{
		{ "_site",       "vm.effect"      },
		{ "room_id",     "!ircd"          },
		{ "type",        "m.room.create"  },
	}
};

void
ircd::m::rooms::create_public_room(const m::event &,
                                   m::vm::eval &)
{
	m::create(public_room_id, m::me.user_id);
}

bool
ircd::m::rooms::_for_each(const string_view &room_id_lb,
                          const room::id::closure_bool &closure)
{
	const room::state state
	{
		my_room
	};

	const room::state::keys_bool keys{[&closure]
	(const string_view &room_id) -> bool
	{
		return closure(room_id);
	}};

	return state.for_each("ircd.room", room_id_lb, keys);
}

size_t
ircd::m::rooms::_count_public(const string_view &server)
{
	const room::state state
	{
		public_room_id
	};

	//TODO: server
	return state.count("ircd.room");
}

bool
ircd::m::rooms::_for_each_public(const string_view &room_id_lb,
                                 const room::id::closure_bool &closure)
{
	const room::state state
	{
		public_room_id
	};

	const room::state::keys_bool keys{[&closure]
	(const string_view &room_id) -> bool
	{
		return closure(room_id);
	}};

	return state.for_each("ircd.room", room_id_lb, keys);
}

void
ircd::m::rooms::_summary_chunk(const m::room &room,
                               json::stack::object &obj)
{
	static const event::keys keys
	{
		event::keys::include
		{
			"content",
		}
	};

    const m::event::fetch::opts fopts
    {
        keys, room.fopts? room.fopts->gopts : db::gopts{}
    };

	const m::room::state state
	{
		room, &fopts
	};

	// Closure over boilerplate primary room state queries; i.e matrix
	// m.room.$type events with state key "" where the content is directly
	// presented to the closure.
	const auto query{[&state]
	(const string_view &type, const string_view &content_key, const auto &closure)
	{
		state.get(std::nothrow, type, "", [&content_key, &closure]
		(const m::event &event)
		{
			const auto &content(json::get<"content"_>(event));
			const auto &value(unquote(content.get(content_key)));
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
			const json::array aliases
			{
				json::get<"content"_>(event).get("aliases")
			};

			for(const string_view &alias : aliases)
				array.append(unquote(alias));
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
