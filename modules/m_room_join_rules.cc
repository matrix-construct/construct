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
               m::vm::eval &eval)
{
	const json::string new_rule
	{
		json::get<"content"_>(event).get("join_rule")
	};

	// Whether the room transitioned from public to private
	const bool privatized
	{
		new_rule == "invite" &&
		m::query(std::nothrow, m::room::state::prev(eval.sequence), "content", false, []
		(const json::object &content)
		{
			const json::string old_rule
			{
				content["join_rule"]
			};

			return old_rule == "public";
		})
	};

	// Delete the entry in the room directory for the no-longer-public room
	if(privatized)
	{
		const m::room::id &room_id
		{
			at<"room_id"_>(event)
		};

		m::rooms::summary::del(room_id);
	}
}

m::hookfn<m::vm::eval &>
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
		string_view{event.event_id},
		json::get<"content"_>(event).get("join_rule"),
	};
}

m::hookfn<m::vm::eval &>
_changed_rules_notify_hookfn
{
	_changed_rules_notify,
	{
		{ "_site",    "vm.notify"          },
		{ "type",     "m.room.join_rules"  },
	}
};
