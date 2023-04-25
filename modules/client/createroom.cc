// Matrix Construct
//
// Copyright (C) Matrix Construct Developers, Authors & Contributors
// Copyright (C) 2016-2018 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

using namespace ircd;

static m::resource::response
post__createroom(client &client,
                 const m::resource::request::object<m::createroom> &request);

mapi::header
IRCD_MODULE
{
	"Client 7.1.1 :Create Room"
};

m::resource
createroom_resource
{
	"/_matrix/client/r0/createRoom",
	{
		"(7.1.1) Create a new room with various configuration options."
	}
};

m::resource::method
post_method
{
	createroom_resource, "POST", post__createroom,
	{
		post_method.REQUIRES_AUTH
	}
};

m::resource::response
post__createroom(client &client,
                 const m::resource::request::object<m::createroom> &request)
{
	m::createroom c
	{
		request
	};

	// unconditionally set/override the room_version here
	json::get<"room_version"_>(c) = string_view
	{
		m::createroom::version_default
	};

	// unconditionally set the creator
	json::get<"creator"_>(c) = request.user_id;

	// unconditionally  generate a room_id
	const m::id::room::buf room_id
	{
		m::id::generate, my_host()
	};

	json::get<"room_id"_>(c) = room_id;

	// conditionally override any preset string that isn't
	// allowed from this client.
	if(!m::createroom::spec_preset(json::get<"preset"_>(c)))
		json::get<"preset"_>(c) = string_view{};

	// the response only contains a room mxid, but we leave some extra buffer
	// to collect any non-fatal error messages accumulated during the process
	const unique_buffer<mutable_buffer> buf
	{
		4_KiB
	};

	json::stack out{buf};
	json::stack::object top
	{
		out
	};

	// note the room_id is already known (and output buffered) before the
	// room creation process has even started
	json::stack::member
	{
		top, "room_id", room_id
	};

	json::stack::array errors
	{
		top, "errors"
	};

	const m::room room
	{
		m::create(c, &errors)
	};

	errors.~array();
	top.~object();
	return m::resource::response
	{
		client, http::OK, json::object
		{
			out.completed()
		}
	};

	return {};
}
