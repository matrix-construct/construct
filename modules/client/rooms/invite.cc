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

namespace ircd::m
{
	static event::id::buf invite_foreign(const event &);
	static void on_invite_foreign(const event &, vm::eval &);

	extern const hookfn<vm::eval &> invite_foreign_hook;
}

decltype(ircd::m::invite_foreign_hook)
ircd::m::invite_foreign_hook
{
	on_invite_foreign,
	{
		{ "_site",          "vm.issue"      },
		{ "type",           "m.room.member" },
		{ "membership",     "invite"        },
	}
};

ircd::resource::response
post__invite(ircd::client &client,
             const ircd::resource::request &request,
             const ircd::m::room::id &room_id)
{
	using namespace ircd;

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
		m::invite(room_id, target, sender)
	};

	return resource::response
	{
		client, http::OK
	};
}

ircd::m::event::id::buf
IRCD_MODULE_EXPORT
ircd::m::invite(const m::room &room,
                const m::user::id &target,
                const m::user::id &sender,
                json::iov &content)
{
	if(!exists(room))
		throw m::NOT_FOUND
		{
			"Not aware of room %s",
			string_view{room.room_id}
		};

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

void
ircd::m::on_invite_foreign(const event &event,
                           vm::eval &eval)
{
	const m::room::id &room_id
	{
		at<"room_id"_>(event)
	};

	const m::user::id &target
	{
		at<"state_key"_>(event)
	};

	const auto target_host
	{
		target.host()
	};

	if(m::my_host(target_host))
		return;

	const m::room::origins origins
	{
		room_id
	};

	if(origins.has(target_host))
		return;

	const auto eid
	{
		invite_foreign(event)
	};
}

ircd::m::event::id::buf
ircd::m::invite_foreign(const event &event)
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

	assert(!my(target));
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
