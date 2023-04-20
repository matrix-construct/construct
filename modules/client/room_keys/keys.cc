// The Construct
//
// Copyright (C) The Construct Developers, Authors & Contributors
// Copyright (C) 2016-2020 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

namespace ircd::m
{
	static string_view make_state_key(const mutable_buffer &, const string_view &, const string_view &, const event::idx &);
	static std::tuple<string_view, string_view, string_view> unmake_state_key(const string_view &);

	static resource::response _get_room_keys_keys(client &, const resource::request &, const room::state &, const event::idx &, const string_view &, const string_view &);
	static void _get_room_keys_keys(client &, const resource::request &, const room::state &, const event::idx &, const string_view &, json::stack::object &);
	static resource::response get_room_keys_keys(client &, const resource::request &);
	extern resource::method room_keys_keys_get;

	static event::id::buf put_room_keys_keys_key(client &, const resource::request &, const room::id &, const string_view &, const event::idx &, const json::object &);
	static resource::response put_room_keys_keys(client &, const resource::request &);
	extern resource::method room_keys_keys_put;

	static event::id::buf delete_room_keys_key(client &, const resource::request &, const room &, const event::idx &);
	static event::id::buf delete_room_keys_key(client &, const resource::request &, const room &, const room::id &, const string_view &, const event::idx &);
	static resource::response delete_room_keys_keys(client &, const resource::request &);
	extern resource::method room_keys_keys_delete;

	extern resource room_keys_keys;
}

ircd::mapi::header
IRCD_MODULE
{
	"Client (undocumented) :e2e Room Keys Keys"
};

decltype(ircd::m::room_keys_keys)
ircd::m::room_keys_keys
{
	"/_matrix/client/unstable/room_keys/keys",
	{
		"(undocumented) Room Keys Keys",
		resource::DIRECTORY,
	}
};

//
// DELETE
//

decltype(ircd::m::room_keys_keys_delete)
ircd::m::room_keys_keys_delete
{
	room_keys_keys, "DELETE", delete_room_keys_keys,
	{
		room_keys_keys_delete.REQUIRES_AUTH |
		room_keys_keys_delete.RATE_LIMITED
	}
};

ircd::m::resource::response
ircd::m::delete_room_keys_keys(client &client,
                               const resource::request &request)
{
	char room_id_buf[room::id::buf::SIZE];
	const string_view &room_id
	{
		request.parv.size() > 0?
			url::decode(room_id_buf, request.parv[0]):
			string_view{}
	};

	char session_id_buf[256];
	const string_view &session_id
	{
		request.parv.size() > 1?
			url::decode(session_id_buf, request.parv[1]):
			string_view{}
	};

	const event::idx version
	{
		request.query.get<event::idx>("version", 0)
	};

	const m::user::room user_room
	{
		request.user_id
	};

	const m::room::state state
	{
		user_room
	};

	if(!room_id && !session_id)
	{
		state.for_each("ircd.room_keys.key", [&client, &request, &user_room, &version]
		(const string_view &, const string_view &state_key, const event::idx &event_idx)
		{
			const auto &[room_id, session_id, _version]
			{
				unmake_state_key(state_key)
			};

			if(version && _version != lex_cast(version))
				return true;

			delete_room_keys_key(client, request, user_room, event_idx);
			return true;
		});
	}
	else if(!session_id)
	{
		state.for_each("ircd.room_keys.key", [&client, &request, &user_room, &version, &room_id]
		(const string_view &, const string_view &state_key, const event::idx &event_idx)
		{
			const auto &[_room_id, session_id, _version]
			{
				unmake_state_key(state_key)
			};

			if(version && _version != lex_cast(version))
				return true;

			if(_room_id != room_id)
				return true;

			delete_room_keys_key(client, request, user_room, event_idx);
			return true;
		});
	}
	else delete_room_keys_key(client, request, user_room, room_id, session_id, version);

	return resource::response
	{
		client, http::OK
	};
}

ircd::m::event::id::buf
ircd::m::delete_room_keys_key(client &client,
                              const resource::request &request,
                              const room &user_room,
                              const room::id &room_id,
                              const string_view &session_id,
                              const event::idx &version)
{
	char state_key_buf[event::STATE_KEY_MAX_SIZE];
	const string_view state_key
	{
		make_state_key(state_key_buf, room_id, session_id, version)
	};

	const room::state state
	{
		user_room
	};

	const auto event_idx
	{
		state.get(std::nothrow, "ircd.room_keys.key", state_key)
	};

	if(!event_idx)
		return {};

	return delete_room_keys_key(client, request, user_room, event_idx);
}

ircd::m::event::id::buf
ircd::m::delete_room_keys_key(client &client,
                              const resource::request &request,
                              const room &user_room,
                              const event::idx &event_idx)
{
	const auto event_id
	{
		m::event_id(event_idx)
	};

	const auto redact_id
	{
		m::redact(user_room, request.user_id, event_id, "deleted by client")
	};

	return redact_id;
}

//
// PUT
//

decltype(ircd::m::room_keys_keys_put)
ircd::m::room_keys_keys_put
{
	room_keys_keys, "PUT", put_room_keys_keys,
	{
		// Flags
		room_keys_keys_put.REQUIRES_AUTH |
		room_keys_keys_put.RATE_LIMITED,

		// timeout //TODO: XXX designated
		30s,

		// Payload maximum
		1_MiB,
	}
};

ircd::m::resource::response
ircd::m::put_room_keys_keys(client &client,
                            const resource::request &request)
{
	char room_id_buf[room::id::buf::SIZE];
	const string_view &room_id
	{
		request.parv.size() > 0?
			url::decode(room_id_buf, request.parv[0]):
			string_view{}
	};

	char session_id_buf[256];
	const string_view &session_id
	{
		request.parv.size() > 1?
			url::decode(session_id_buf, request.parv[1]):
			string_view{}
	};

	const event::idx version
	{
		request.query.at<event::idx>("version")
	};

	if(!room_id && !session_id)
	{
		const json::object &rooms
		{
			request["rooms"]
		};

		for(const auto &[room_id, room_data] : rooms)
		{
			const json::object sessions
			{
				json::object(room_data)["sessions"]
			};

			for(const auto &[session_id, session] : sessions)
				put_room_keys_keys_key(client, request, room_id, session_id, version, session);
		}
	}
	else if(!session_id)
	{
		const json::object &sessions
		{
			request["sessions"]
		};

		for(const auto &[session_id, session] : sessions)
			put_room_keys_keys_key(client, request, room_id, session_id, version, session);
	}
	else put_room_keys_keys_key(client, request, room_id, session_id, version, request);

	return resource::response
	{
		client, http::OK
	};
}

ircd::m::event::id::buf
ircd::m::put_room_keys_keys_key(client &client,
                                const resource::request &request,
                                const room::id &room_id,
                                const string_view &session_id,
                                const event::idx &version,
                                const json::object &content)
{
	const m::user::room user_room
	{
		request.user_id
	};

	const m::room::type events
	{
		user_room, "ircd.room_keys.version"
	};

	events.for_each([&version]
	(const auto &, const auto &, const event::idx &_event_idx)
	{
		if(m::redacted(_event_idx))
			return true;

		if(_event_idx != version)
			throw http::error
			{
				"%lu is not the most recent key version",
				http::FORBIDDEN,
				version
			};

		return false; // false to break after this first hit
	});

	char state_key_buf[event::STATE_KEY_MAX_SIZE];
	const string_view state_key
	{
		make_state_key(state_key_buf, room_id, session_id, version)
	};

	const auto event_id
	{
		send(user_room, request.user_id, "ircd.room_keys.key", state_key, content)
	};

	return event_id;
}

//
// GET
//

decltype(ircd::m::room_keys_keys_get)
ircd::m::room_keys_keys_get
{
	room_keys_keys, "GET", get_room_keys_keys,
	{
		room_keys_keys_get.REQUIRES_AUTH |
		room_keys_keys_get.RATE_LIMITED
	}
};

ircd::m::resource::response
ircd::m::get_room_keys_keys(client &client,
                            const resource::request &request)
{
	char room_id_buf[room::id::buf::SIZE];
	const string_view &room_id
	{
		request.parv.size() > 0?
			url::decode(room_id_buf, request.parv[0]):
			string_view{}
	};

	char session_id_buf[256];
	const string_view &session_id
	{
		request.parv.size() > 1?
			url::decode(session_id_buf, request.parv[1]):
			string_view{}
	};

	const event::idx version
	{
		request.query.at<event::idx>("version")
	};

	const m::user::room user_room
	{
		request.user_id
	};

	const m::room::state state
	{
		user_room
	};

	if(room_id && session_id)
		return _get_room_keys_keys(client, request, state, version, room_id, session_id);

	resource::response::chunked response
	{
		client, http::OK
	};

	json::stack out
	{
		response.buf, response.flusher()
	};

	json::stack::object top
	{
		out
	};

	json::stack::object rooms
	{
		top, "rooms"
	};

	if(room_id)
	{
		_get_room_keys_keys(client, request, state, version, room_id, rooms);
		return response;
	}

	m::room::id::buf last_room;
	state.for_each("ircd.room_keys.key", [&client, &request, &state, &version, &rooms, &last_room]
	(const string_view &, const string_view &state_key, const event::idx &)
	{
		const auto &[room_id, _session_id, _version]
		{
			unmake_state_key(state_key)
		};

		if(_version != lex_cast(version))
			return true;

		if(room_id == last_room)
			return true;

		_get_room_keys_keys(client, request, state, version, room_id, rooms);
		return true;
	});

	return response;
}

void
ircd::m::_get_room_keys_keys(client &client,
                             const resource::request &request,
                             const m::room::state &state,
                             const event::idx &version,
                             const string_view &room_id,
                             json::stack::object &rooms)
{
	json::stack::object room
	{
		rooms, room_id
	};

	json::stack::object sessions
	{
		room, "sessions"
	};

	state.for_each("ircd.room_keys.key", [&room_id, &version, &sessions]
	(const string_view &type, const string_view &state_key, const event::idx &event_idx)
	{
		const auto &[_room_id, _session_id, _version]
		{
			unmake_state_key(state_key)
		};

		if(_room_id != room_id)
			return true;

		if(_version != lex_cast(version))
			return true;

		const string_view &session_id
		{
			_session_id
		};

		m::get(std::nothrow, event_idx, "content", [&sessions, &session_id]
		(const json::object &session)
		{
			json::stack::member
			{
				sessions, session_id, session
			};
		});

		return true;
	});
}

ircd::m::resource::response
ircd::m::_get_room_keys_keys(client &client,
                             const resource::request &request,
                             const m::room::state &state,
                             const event::idx &version,
                             const string_view &room_id,
                             const string_view &session_id)
{
	char state_key_buf[event::STATE_KEY_MAX_SIZE];
	const string_view state_key
	{
		make_state_key(state_key_buf, room_id, session_id, version)
	};

	const auto event_idx
	{
		state.get("ircd.room_keys.key", state_key)
	};

	m::get(event_idx, "content", [&client]
	(const json::object &content)
	{
		resource::response
		{
			client, content
		};
	});

	return {}; // responded from closure or thrown
}

std::tuple<ircd::string_view, ircd::string_view, ircd::string_view>
ircd::m::unmake_state_key(const string_view &state_key)
{
	assert(state_key);
	string_view part[3];
	const auto parts
	{
		tokens(state_key, ":::", part)
	};

	assert(parts == 3);
	if(unlikely(!m::valid(id::ROOM, part[0])))
		part[0] = {};

	if(unlikely(!lex_castable<ulong>(part[2])))
		part[2] = {};

	return std::make_tuple
	(
		part[0], part[1], part[2]
	);
}

ircd::string_view
ircd::m::make_state_key(const mutable_buffer &buf,
                        const string_view &room_id,
                        const string_view &session_id,
                        const event::idx &version)
{
	assert(room_id);
	assert(m::valid(id::ROOM, room_id));
	assert(session_id);
	assert(session_id != "sessions");
	assert(version != 0);
	return fmt::sprintf
	{
		buf, "%s:::%s:::%u",
		room_id,
		session_id,
		version,
	};
}
