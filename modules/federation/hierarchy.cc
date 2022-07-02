// The Construct
//
// Copyright (C) The Construct Developers, Authors & Contributors
// Copyright (C) 2016-2022 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

using namespace ircd;

static m::resource::response
get__hierarchy(client &client,
               const m::resource::request &request);

mapi::header
IRCD_MODULE
{
	"Federation 16 :Spaces"
};

m::resource
hierarchy_resource
{
	"/_matrix/federation/v1/hierarchy/",
	{
		"Federation 16 :Spaces hierarchy.",
		resource::DIRECTORY,
	}
};

m::resource::method
method_get
{
	hierarchy_resource, "GET", get__hierarchy,
	{
		method_get.VERIFY_ORIGIN
	}
};

conf::item<size_t>
flush_hiwat
{
	{ "name",     "ircd.federation.hierarchy.flush.hiwat" },
	{ "default",  16384L                                  },
};

conf::item<size_t>
inaccessible_limit
{
	{ "name",     "ircd.federation.hierarchy.inaccessible.limit" },
	{ "default",  1024L                                          },
};

m::resource::response
get__hierarchy(client &client,
               const m::resource::request &request)
{
	using server_acl = m::room::server_acl;

	if(request.parv.size() < 1)
		throw m::NEED_MORE_PARAMS
		{
			"room_id path parameter required"
		};

	m::room::id::buf room_id
	{
		url::decode(room_id, request.parv[0])
	};

	const m::room room
	{
		room_id
	};

	if(server_acl::enable_read && !server_acl::check(room_id, request.node_id))
		throw m::ACCESS_DENIED
		{
			"You are not permitted by the room's server access control list."
		};

	if(!visible(room, request.node_id))
		throw m::ACCESS_DENIED
		{
			"You are not permitted to view the room at this event"
		};

	const bool suggested_only
	{
		request.query.get("suggested_only", false)
	};

	const m::room::state state
	{
		room
	};

	m::resource::response::chunked response
	{
		client, http::OK
	};

	json::stack out
	{
		response.buf, response.flusher(), flush_hiwat
	};

	json::stack::object top
	{
		out
	};

	// room
	{
		json::stack::object object
		{
			top, "room"
		};

		m::rooms::summary::get(object, room);
	}

	// children
	std::vector<std::string> inaccessible;
	{
		json::stack::object object
		{
			top, "children"
		};

		state.for_each("m.space.child", [&object, &request, &inaccessible]
		(const string_view &, const string_view &state_key, const m::event::idx &)
		{
			if(!m::valid(m::id::ROOM, state_key))
				return true;

			const m::room::id room_id
			{
				state_key
			};

			const bool accessible
			{
				true
				&& m::exists(room_id)
				&& (!server_acl::enable_read || server_acl::check(room_id, request.node_id))
				&& visible(room_id, request.node_id)
			};

			if(!accessible && likely(inaccessible.size() < inaccessible_limit))
				inaccessible.emplace_back(room_id);

			if(accessible)
				m::rooms::summary::get(object, room_id);

			return true;
		});
	}

	// inaccessible_children
	{
		json::stack::array array
		{
			top, "inaccessible_children"
		};

		for(const string_view room_id : inaccessible)
			array.append(room_id);
	}

	return response;
}
