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

mapi::header
IRCD_MODULE
{
	"Matrix m.room.create"
};

static void
_can_create_room(const m::event &event,
                 m::vm::eval &eval)
{
	const m::event::id &event_id
	{
		at<"event_id"_>(event)
	};

	const m::room::id &room_id
	{
		at<"room_id"_>(event)
	};

	const m::user::id &sender
	{
		at<"sender"_>(event)
	};

	if(room_id.host() != sender.host())
		throw m::ACCESS_DENIED
		{
			"Room on '%s' cannot be created by sender on '%s'",
			room_id.host(),
			sender.host()
		};

	if(room_id.host() != event_id.host())
		throw m::ACCESS_DENIED
		{
			"Room on '%s' cannot be created by event from '%s'",
			room_id.host(),
			event_id.host()
		};

	if(room_id.host() != at<"origin"_>(event))
		throw m::ACCESS_DENIED
		{
			"Room on '%s' cannot be created by event originating from '%s'",
			room_id.host(),
			at<"origin"_>(event)
		};

	const json::object &content
	{
		at<"content"_>(event)
	};

	// note: this is a purposely weak check of the content.creator field.
	// This server accepts a missing content.creator field entirely because
	// it considers the sender of the create event as the room creator.
	// Nevertheless if the content.creator field was specified it must match
	// the sender or we reject.
	if(content.has("creator"))
		if(unquote(content.at("creator")) != sender)
			throw m::ACCESS_DENIED
			{
				"Room %s creator must be the sender %s",
				string_view{room_id},
				string_view{sender}
			};
}

const m::hookfn<m::vm::eval &>
_can_create_room_hookfn
{
	_can_create_room,
	{
		{ "_site",    "vm.eval"       },
		{ "type",     "m.room.create" },
	}
};

static void
_created_room(const m::event &event,
              m::vm::eval &)
{
	const m::room::id &room_id
	{
		at<"room_id"_>(event)
	};

	const string_view &local
	{
		room_id.localname()
	};

	if(local != "users") //TODO: circ dep
		send(m::my_room, at<"sender"_>(event), "ircd.room", room_id, json::object{});

	log::debug
	{
		"Creation of room %s by %s (%s)",
		string_view{room_id},
		at<"sender"_>(event),
		at<"event_id"_>(event)
	};
}

const m::hookfn<m::vm::eval &>
_created_room_hookfn
{
	_created_room,
	{
		{ "_site",    "vm.effect"     },
		{ "type",     "m.room.create" },
	}
};
