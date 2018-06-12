// Matrix Construct
//
// Copyright (C) Matrix Construct Developers, Authors & Contributors
// Copyright (C) 2016-2018 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

#include "rooms.h"

using namespace ircd;

extern "C" m::event::id::buf
invite__room_user(const m::room &,
                  const m::user::id &target,
                  const m::user::id &sender);

extern "C" m::event::id::buf
invite__foreign(const m::event &);

resource::response
post__invite(client &client,
             const resource::request &request,
             const m::room::id &room_id)
{
	const m::user::id &target
	{
		unquote(request.at("user_id"))
	};

	const m::user::id &sender
	{
		request.user_id
	};

	const auto event_id
	{
		invite__room_user(room_id, target, sender)
	};

	return resource::response
	{
		client, http::OK
	};
}

m::event::id::buf
invite__room_user(const m::room &room,
                  const m::user::id &target,
                  const m::user::id &sender)
{
	if(!exists(room))
		throw m::NOT_FOUND
		{
			"Not aware of room %s",
			string_view{room.room_id}
		};

	json::iov event;
	json::iov content;
	const json::iov::push push[]
	{
		{ event,    { "type",        "m.room.member"  }},
		{ event,    { "sender",      sender           }},
		{ event,    { "state_key",   target           }},
		{ event,    { "membership",  "invite"         }},
		{ content,  { "membership",  "invite"         }},
	};

	return commit(room, event, content);
}

extern "C" m::event::id::buf
invite__foreign(const m::event &event)
{
	const auto &event_id
	{
		at<"event_id"_>(event)
	};

	const auto &room_id
	{
		at<"room_id"_>(event)
	};

	const m::user::id &target
	{
		at<"state_key"_>(event)
	};

	const unique_buffer<mutable_buffer> bufs
	{
		148_KiB
	};

	mutable_buffer buf{bufs};
	const auto proto
	{
		json::stringify(buf, event)
	};

	m::v1::invite::opts opts;
	opts.remote = target.host();
	m::v1::invite request
	{
		room_id, event_id, proto, buf, std::move(opts)
	};

	request.wait(seconds(10)); //TODO: conf
	request.get();

	const json::array &response
	{
		request.in.content
	};

	const http::code &rcode
	{
		http::code(lex_cast<ushort>(response.at(0)))
	};

	if(rcode != http::OK)
		throw http::error
		{
			rcode
		};

	const json::object &robject
	{
		response.at(1)
	};

	const m::event &revent
	{
		robject.at("event")
	};

	if(!verify(revent, target.host()))
		throw m::error
		{
			http::UNAUTHORIZED, "M_INVITE_UNSIGNED",
			"Invitee's host '%s' did not sign the invite.",
			target.host()
		};

	if(!verify(revent, my_host()))
		throw m::error
		{
			http::FORBIDDEN, "M_INVITE_MODIFIED",
			"Invite event no longer verified by our signature."
		};

	m::vm::opts vmopts;
	vmopts.non_conform.set(m::event::conforms::MISSING_PREV_STATE);
	vmopts.infolog_accept = true;

	m::vm::eval(revent, vmopts);
	return at<"event_id"_>(revent);
}
