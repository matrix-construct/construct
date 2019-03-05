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

mapi::header
IRCD_MODULE
{
	"Client 7.1.1 :Create Room"
};

extern "C" room
createroom__parent_type(const id::room &room_id,
                        const id::user &creator,
                        const id::room &parent,
                        const string_view &type);

extern "C" room
createroom__type(const id::room &room_id,
                 const id::user &creator,
                 const string_view &type);

extern "C" room
createroom_(const id::room &room_id,
            const id::user &creator);

const room::id::buf
init_room_id
{
	"init", ircd::my_host()
};

resource
createroom_resource
{
	"/_matrix/client/r0/createRoom",
	{
		"(7.1.1) Create a new room with various configuration options."
	}
};

struct report_error
{
	static thread_local char buf[512];

	template<class... args>
	report_error(json::stack::array &errors,
	             const string_view &room_id,
	             const string_view &user_id,
	             const string_view &fmt,
	             args&&... a);
};

resource::response
post__createroom(client &client,
                 const resource::request::object<m::createroom> &request)
try
{
	const id::user &sender_id
	{
		request.user_id
	};

	const id::room::buf room_id
	{
		id::generate, my_host()
	};

	const room room
	{
		createroom_(room_id, sender_id)
	};

	resource::response::chunked response
	{
		client, http::CREATED, 2_KiB
	};

	json::stack out
	{
		response.buf, response.flusher()
	};

	json::stack::object top
	{
		out
	};

	json::stack::member
	{
		top, "room_id", room.room_id
	};

	json::stack::array errors
	{
		top, "errors"
	};

	const event::id::buf join_event_id
	{
		join(room, sender_id)
	};

	thread_local char pl_content_buf[4_KiB]; try
	{
		send(room, sender_id, "m.room.power_levels", "",
		{
			json::get<"power_level_content_override"_>(request)?
				json::get<"power_level_content_override"_>(request):
				m::room::power::default_content(pl_content_buf, sender_id)
		});
	}
	catch(const std::exception &e)
	{
		report_error
		{
			errors, room_id, sender_id, "Failed to set power_levels: %s", e.what()
		};
	}

	const json::string preset
	{
		json::get<"preset"_>(request)
	};

	const string_view &join_rule
	{
		preset == "private_chat"?         "invite":
		preset == "trusted_private_chat"? "invite":
		preset == "public_chat"?          "public":
		                                  "invite"
	};

	if(join_rule != "invite") try
	{
		send(room, sender_id, "m.room.join_rules", "",
		{
			{ "join_rule", join_rule }
		});
	}
	catch(const std::exception &e)
	{
		report_error
		{
			errors, room_id, sender_id, "Failed to set join_rules: %s", e.what()
		};
	}

	const string_view &history_visibility
	{
		preset == "private_chat"?         "shared":
		preset == "trusted_private_chat"? "shared":
		preset == "public_chat"?          "shared":
		                                  "shared"
	};

	if(history_visibility != "shared") try
	{
		send(room, sender_id, "m.room.history_visibility", "",
		{
			{ "history_visibility", history_visibility }
		});
	}
	catch(const std::exception &e)
	{
		report_error
		{
			errors, room_id, sender_id, "Failed to set history_visibility: %s", e.what()
		};
	}

	const string_view &guest_access
	{
		preset == "private_chat"?         "can_join":
		preset == "trusted_private_chat"? "can_join":
		preset == "public_chat"?          "forbidden":
		                                  "forbidden"
	};

	if(guest_access == "can_join") try
	{
		send(room, sender_id, "m.room.guest_access", "",
		{
			{ "guest_access", "can_join" }
		});
	}
	catch(const std::exception &e)
	{
		report_error
		{
			errors, room_id, sender_id, "Failed to set guest_access: %s", e.what()
		};
	}

	// Takes precedence over events set by preset, but gets overriden by name
	// and topic keys.
	size_t i(0);
	for(const json::object &event : json::get<"initial_state"_>(request)) try
	{
		const json::string &type(event["type"]);
		const json::string &state_key(event["state_key"]);
		const json::object &content(event["content"]);
		send(room, sender_id, type, state_key, content);
		++i;
	}
	catch(const std::exception &e)
	{
		report_error
		{
			errors, room_id, sender_id, "Failed to set initial_state event @%zu: %s",
			i++,
			e.what()
		};
	}

	if(json::get<"name"_>(request)) try
	{
		static const size_t name_max_len
		{
			// 14.2.1.3: The name of the room. This MUST NOT exceed 255 bytes.
			255
		};

		const auto name
		{
			trunc(json::get<"name"_>(request), name_max_len)
		};

		send(room, sender_id, "m.room.name", "",
		{
			{ "name", name }
		});
	}
	catch(const std::exception &e)
	{
		report_error
		{
			errors, room_id, sender_id, "Failed to set room name: %s", e.what()
		};
	}

	if(json::get<"topic"_>(request)) try
	{
		send(room, sender_id, "m.room.topic", "",
		{
			{ "topic", json::get<"topic"_>(request) }
		});
	}
	catch(const std::exception &e)
	{
		report_error
		{
			errors, room_id, sender_id, "Failed to set room topic: %s", e.what()
		};
	}

	for(const json::string &_user_id : json::get<"invite"_>(request)) try
	{
		const m::user::id &user_id{_user_id};
		invite(room, user_id, sender_id);
	}
	catch(const std::exception &e)
	{
		report_error
		{
			errors, room_id, sender_id, "Failed to invite user '%s': %s",
			_user_id,
			e.what()
		};
	}

	if(json::get<"guest_can_join"_>(request) && guest_access != "can_join") try
	{
		send(room, sender_id, "m.room.guest_access", "",
		{
			{ "guest_access", "can_join" }
		});
	}
	catch(const std::exception &e)
	{
		report_error
		{
			errors, room_id, sender_id, "Failed to set guest_access: %s", e.what()
		};
	}

	if(json::get<"visibility"_>(request) == "public") try
	{
		// This call sends a message to the !public room to list this room in the
		// public rooms list. We set an empty summary for this room because we
		// already have its state on this server;
		m::rooms::summary_set(room.room_id, json::object{});
	}
	catch(const std::exception &e)
	{
		report_error
		{
			errors, room_id, sender_id, "Failed to set public visibility: %s", e.what()
		};
	}

	return {};
}
catch(const db::not_found &e)
{
	throw m::error
	{
		http::CONFLICT, "M_ROOM_IN_USE", "The desired room name is in use."
	};

	throw m::error
	{
		http::CONFLICT, "M_ROOM_ALIAS_IN_USE", "An alias of the desired room is in use."
	};
}

resource::method
post_method
{
	createroom_resource, "POST", post__createroom,
	{
		post_method.REQUIRES_AUTH
	}
};

room
createroom_(const id::room &room_id,
            const id::user &creator)
{
	return createroom__type(room_id, creator, string_view{});
}

room
createroom__type(const id::room &room_id,
                 const id::user &creator,
                 const string_view &type)
{
	return createroom__parent_type(room_id, creator, init_room_id, type);
}

room
createroom__parent_type(const id::room &room_id,
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

	const json::iov::add _parent
	{
		content, !parent.empty() && parent.local() != "init",
		{
			"parent", [&parent]() -> json::value
			{
				return parent;
			}
		}
	};

	const json::iov::add _type
	{
		content, !type.empty() && type != "room",
		{
			"type", [&type]() -> json::value
			{
				return type;
			}
		}
	};

	const json::iov::push _push[]
	{
		{ event,  { "depth",       0L               }},
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

decltype(report_error::buf) thread_local
report_error::buf;

template<class... args>
report_error::report_error(json::stack::array &errors,
                           const string_view &room_id,
                           const string_view &user_id,
                           const string_view &fmt,
                           args&&... a)
{
	const string_view msg{fmt::sprintf
	{
		buf, fmt, std::forward<args>(a)...
	}};

	log::derror
	{
		m::log, "Error when creating room %s for user %s :%s",
		room_id,
		user_id,
		msg
	};

	errors.append(msg);
}
