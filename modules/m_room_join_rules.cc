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
	"Matrix m.room.join_rules"
};

static void
_changed_rules(const m::event &event,
               m::vm::eval &)
{
	const m::user::id &sender
	{
		at<"sender"_>(event)
	};

	if(!my(sender))
		return;

	const m::room::id::buf public_room
	{
		"public", my_host()
	};

	const m::room::id &room_id
	{
		at<"room_id"_>(event)
	};

	// This call sends a message to the !public room to list this room in the
	// public rooms list. We set an empty summary for this room because we
	// already have its state on this server; saving a summary object in the
	// event sent to !public is only for rooms whose state is not synced.
	m::rooms::summary_set(room_id, json::object{});
}

const m::hookfn<m::vm::eval &>
_changed_rules_hookfn
{
	_changed_rules,
	{
		{ "_site",    "vm.effect"          },
		{ "type",     "m.room.join_rules"  },
	}
};

static void
_changed_rules_notify(const m::event &event,
                      m::vm::eval &)
{
	log::info
	{
		m::log, "%s changed join_rules in %s [%s] to %s",
		json::get<"sender"_>(event),
		json::get<"room_id"_>(event),
		json::get<"event_id"_>(event),
		json::get<"content"_>(event).get("join_rule"),
	};
}

const m::hookfn<m::vm::eval &>
_changed_rules_notify_hookfn
{
	_changed_rules_notify,
	{
		{ "_site",    "vm.notify"          },
		{ "type",     "m.room.join_rules"  },
	}
};
