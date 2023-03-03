// The Construct
//
// Copyright (C) The Construct Developers, Authors & Contributors
// Copyright (C) 2016-2020 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

namespace ircd::m::admin
{
	static resource::response handle_get_members(client &, const resource::request &, const room::id &);
	static resource::response handle_delete_forward_extremis(client &, const resource::request &, const room::id &);
	static resource::response handle_get_forward_extremis(client &, const resource::request &, const room::id &);
	static resource::response handle(client &, const resource::request &);

	extern resource::method get_method;
	extern resource::method delete_method;
	extern resource rooms_resource;
};

ircd::mapi::header
IRCD_MODULE
{
	"Admin (undocumented) :Rooms"
};

decltype(ircd::m::admin::rooms_resource)
ircd::m::admin::rooms_resource
{
	"/_synapse/admin/v1/rooms/",
	{
		"(undocumented) Admin Rooms",
		resource::DIRECTORY
	}
};

decltype(ircd::m::admin::get_method)
ircd::m::admin::get_method
{
	rooms_resource, "GET", handle,
	{
		get_method.REQUIRES_OPER
	}
};

decltype(ircd::m::admin::delete_method)
ircd::m::admin::delete_method
{
	rooms_resource, "DELETE", handle,
	{
		delete_method.REQUIRES_OPER
	}
};

ircd::m::resource::response
ircd::m::admin::handle(client &client,
                       const resource::request &request)
{
	char buf[768];
	const string_view &room_id_or_alias
	{
		request.parv[0]?
			url::decode(buf, request.parv[0]):
			string_view{}
	};

	const m::room::id::buf room_id
	{
		room_id_or_alias?
			m::room_id(room_id_or_alias):
			m::room::id::buf{}
	};

	const auto &cmd
	{
		request.parv[1]
	};

	if(request.head.method == "DELETE" && cmd == "forward_extremities")
		return handle_delete_forward_extremis(client, request, room_id);

	if(request.head.method == "GET" && cmd == "forward_extremities")
		return handle_get_forward_extremis(client, request, room_id);

	if(request.head.method == "GET" && cmd == "members")
		return handle_get_members(client, request, room_id);

	throw m::NOT_FOUND
	{
		"/admin/rooms command not found"
	};
}

ircd::m::resource::response
ircd::m::admin::handle_delete_forward_extremis(client &client,
                                               const resource::request &request,
                                               const room::id &room_id)
{
	const room::head room_head
	{
		room_id
	};

	return resource::response
	{
		client, http::OK, json::members
		{
			{ "deleted", long(room::head::reset(room_head)) }
		}
	};
}

ircd::m::resource::response
ircd::m::admin::handle_get_forward_extremis(client &client,
                                            const resource::request &request,
                                            const room::id &room_id)
{
	const m::room::head room_head
	{
		room_id
	};

	m::resource::response::chunked::json response
	{
		client, http::OK
	};

	json::stack::member
	{
		response, "count", json::value
		{
			long(room_head.count())
		}
	};

	json::stack::array results
	{
		response, "results"
	};

	room_head.for_each([&results]
	(const auto &event_idx, const auto &event_id)
	{
		json::stack::object result
		{
			results
		};

		json::stack::member
		{
			result, "event_id", event_id
		};

		json::stack::member
		{
			result, "depth", json::value
			{
				m::get(std::nothrow, event_idx, "depth", 0L)
			}
		};

		return true;
	});

	return response;
}

ircd::m::resource::response
ircd::m::admin::handle_get_members(client &client,
                                   const resource::request &request,
                                   const room::id &room_id)
{
	const m::room::members members
	{
		room_id
	};

	m::resource::response::chunked::json response
	{
		client, http::OK
	};

	json::stack::member
	{
		response, "total", json::value
		{
			long(members.count("join"))
		}
	};

	json::stack::array array
	{
		response, "members"
	};

	members.for_each("join", [&array]
	(const m::id::user &user_id)
	{
		array.append(user_id);
		return true;
	});

	return response;
}
