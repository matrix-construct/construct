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
	static resource::response get_room_keys_version(client &, const resource::request &);
	extern resource::method room_keys_version_get;

	static resource::response put_room_keys_version(client &, const resource::request &);
	extern resource::method room_keys_version_put;

	static resource::response delete_room_keys_version(client &, const resource::request &);
	extern resource::method room_keys_version_delete;

	static resource::response post_room_keys_version(client &, const resource::request &);
	extern resource::method room_keys_version_post;

	extern resource room_keys_version;
}

ircd::mapi::header
IRCD_MODULE
{
	"Client (undocumented) :e2e Room Keys Version"
};

decltype(ircd::m::room_keys_version)
ircd::m::room_keys_version
{
	"/_matrix/client/unstable/room_keys/version",
	{
		"(undocumented) Room Keys Version",
		resource::DIRECTORY,
	}
};

//
// POST
//

decltype(ircd::m::room_keys_version_post)
ircd::m::room_keys_version_post
{
	room_keys_version, "POST", post_room_keys_version,
	{
		room_keys_version_post.REQUIRES_AUTH
	}
};

ircd::m::resource::response
ircd::m::post_room_keys_version(client &client,
                                const resource::request &request)
{
	const json::string &algorithm
	{
		request["algorithm"]
	};

	const json::object &auth_data
	{
		request["auth_data"]
	};

	const json::string &public_key
	{
		auth_data["public_key"]
	};

	const json::object &signatures
	{
		auth_data["signatures"]
	};

	const m::device::id::buf device_id
	{
		m::user::tokens::device(request.access_token)
	};

	const m::user::room user_room
	{
		request.user_id
	};

	const auto event_id
	{
		m::send(user_room, request.user_id, "ircd.room_keys.version", json::object
		{
			request
		})
	};

	const json::value version
	{
		lex_cast(m::index(event_id)), json::STRING
	};

	return resource::response
	{
		client, json::members
		{
			{ "version", version }
		}
	};
}

//
// DELETE
//

decltype(ircd::m::room_keys_version_delete)
ircd::m::room_keys_version_delete
{
	room_keys_version, "DELETE", delete_room_keys_version,
	{
		room_keys_version_delete.REQUIRES_AUTH
	}
};

ircd::m::resource::response
ircd::m::delete_room_keys_version(client &client,
                                  const resource::request &request)
{
	if(request.parv.size() < 1)
		throw m::NEED_MORE_PARAMS
		{
			"version path parameter required",
		};

	const m::user::room user_room
	{
		request.user_id
	};

	const event::idx event_idx
	{
		lex_cast<event::idx>(request.parv[0])
	};

	if(m::room_id(event_idx) != user_room.room_id)
		throw m::ACCESS_DENIED
		{
			"Event idx:%lu is not in your room",
			event_idx,
		};

	const auto event_id
	{
		m::event_id(event_idx)
	};

	const auto redact_id
	{
		m::redact(user_room, request.user_id, event_id, "deleted by client")
	};

	return resource::response
	{
		client, http::OK
	};
}

//
// PUT
//

decltype(ircd::m::room_keys_version_put)
ircd::m::room_keys_version_put
{
	room_keys_version, "PUT", put_room_keys_version,
	{
		room_keys_version_put.REQUIRES_AUTH
	}
};

ircd::m::resource::response
ircd::m::put_room_keys_version(client &client,
                                  const resource::request &request)
{

	return resource::response
	{
		client, http::NOT_IMPLEMENTED
	};
}

//
// GET
//

decltype(ircd::m::room_keys_version_get)
ircd::m::room_keys_version_get
{
	room_keys_version, "GET", get_room_keys_version,
	{
		room_keys_version_get.REQUIRES_AUTH
	}
};

ircd::m::resource::response
ircd::m::get_room_keys_version(client &client,
                               const resource::request &request)
{
	const m::user::room user_room
	{
		request.user_id
	};

	event::idx event_idx
	{
		request.parv.size() >= 1?
			lex_cast<event::idx>(request.parv[0]):
			0UL
	};

	if(!event_idx)
	{
		const m::room::type events
		{
			user_room, "ircd.room_keys.version"
		};

		events.for_each([&event_idx]
		(const auto &, const auto &, const event::idx &_event_idx)
		{
			if(m::redacted(_event_idx))
				return true;

			event_idx = _event_idx;
			return false; // false to break after this first hit
		});
	}

	if(!event_idx)
		return resource::response
		{
			client, http::NOT_FOUND
		};

	if(m::room_id(event_idx) != user_room.room_id)
		throw m::ACCESS_DENIED
		{
			"Event idx:%lu is not in your room",
			event_idx,
		};

	if(m::redacted(event_idx))
		return resource::response
		{
			client, http::NOT_FOUND
		};

	m::get(event_idx, "content", [&client, &event_idx]
	(const json::object &content)
	{
		const json::value version
		{
			lex_cast(event_idx), json::STRING
		};

		resource::response
		{
			client, json::members
			{
				{ "version",    version              },
				{ "algorithm",  content["algorithm"] },
				{ "auth_data",  content["auth_data"] },
			}
		};
	});

	return {}; // Responded from closure
}
