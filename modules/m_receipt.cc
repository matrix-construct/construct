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
	"Matrix Receipts"
};

extern "C" bool last_receipt__event_id(const m::room::id &, const m::user::id &, const m::event::id::closure &);
static void handle_m_receipt_m_read(const m::room::id &, const m::user::id &, const m::event::id &, const time_t &);
static void handle_m_receipt_m_read(const m::room::id &, const m::user::id &, const m::edu::m_receipt::m_read &);
static void handle_m_receipt_m_read(const m::room::id &, const json::object &);
static void handle_m_receipt(const m::room::id &, const json::object &);
static void handle_edu_m_receipt(const m::event &, m::vm::eval &);

const m::hookfn<m::vm::eval &>
_m_receipt_eval
{
	handle_edu_m_receipt,
	{
		{ "_site",   "vm.eval"    },
		{ "type",    "m.receipt"  },
	}
};

void
handle_edu_m_receipt(const m::event &event,
                     m::vm::eval &eval)
{
	if(json::get<"room_id"_>(event))
		return;

	const json::object &content
	{
		at<"content"_>(event)
	};

	for(const auto &member : content)
	{
		const m::room::id &room_id
		{
			unquote(member.first)
		};

		handle_m_receipt(room_id, member.second);
	}
}

void
handle_m_receipt(const m::room::id &room_id,
                 const json::object &content)
{
	for(const auto &member : content)
	{
		const string_view &type
		{
			unquote(member.first)
		};

		if(type == "m.read")
		{
			handle_m_receipt_m_read(room_id, member.second);
			continue;
		}

		log::dwarning
		{
			"Unhandled m.receipt type '%s' to room '%s'",
			type,
			string_view{room_id}
		};
	}
}

void
handle_m_receipt_m_read(const m::room::id &room_id,
                        const json::object &content)
{
	for(const auto &member : content)
	{
		const m::user::id user_id
		{
			unquote(member.first)
		};

		const json::object &content
		{
			member.second
		};

		handle_m_receipt_m_read(room_id, user_id, content);
	}
}

void
handle_m_receipt_m_read(const m::room::id &room_id,
                        const m::user::id &user_id,
                        const m::edu::m_receipt::m_read &receipt)
{
	const json::array &event_ids
	{
		json::get<"event_ids"_>(receipt)
	};

	const json::object &data
	{
		json::get<"data"_>(receipt)
	};

	const time_t &ts
	{
		data.get<time_t>("ts")
	};

	for(const string_view &event_id : event_ids)
		handle_m_receipt_m_read(room_id, user_id, unquote(event_id), ts);
}

void
handle_m_receipt_m_read(const m::room::id &room_id,
                        const m::user::id &user_id,
                        const m::event::id &event_id,
                        const time_t &ts)
try
{
	const m::user user
	{
		user_id
	};

	if(!exists(user))
	{
		log::dwarning
		{
			"ignoring m.receipt m.read for unknown %s in %s for %s",
			string_view{user_id},
			string_view{room_id},
			string_view{event_id}
		};

		return;
	}

	const m::user::room user_room
	{
		user
	};

	const auto evid
	{
		send(user_room, user_id, "ircd.read", room_id,
		{
			{ "event_id", event_id },
			{ "ts",       ts       }
		})
	};

	m::event receipt;
	json::get<"depth"_>(receipt) = json::undefined_number;
	json::get<"type"_>(receipt) = "m.receipt"_sv;
	json::get<"room_id"_>(receipt) = room_id;
	json::get<"sender"_>(receipt) = user_id;
	json::get<"origin"_>(receipt) = user_id.host();

	char buf[768];
	json::get<"content"_>(receipt) = json::stringify(mutable_buffer{buf}, json::members
	{
		{ event_id,
		{
			{ "m.read",
			{
				{ user_id,
				{
					{ "ts", ts }
				}}
			}}
		}}
	});

	m::vm::opts vmopts;
	vmopts.conforming = false;
	m::vm::eval eval
	{
		receipt, vmopts
	};

	log::info
	{
		"%s read by %s in %s @ %zd",
		string_view{event_id},
		string_view{user_id},
		string_view{room_id},
		ts
	};
}
catch(const std::exception &e)
{
	log::derror
	{
		"failed to save m.receipt m.read for %s in %s for %s :%s",
		string_view{user_id},
		string_view{room_id},
		string_view{event_id},
		e.what()
	};
}

bool
last_receipt__event_id(const m::room::id &room_id,
                       const m::user::id &user_id,
                       const m::event::id::closure &closure)
{
	static const m::event::fetch::opts fopts
	{
		m::event::keys::include
		{
			"content"
		}
	};

	const m::user::room user_room
	{
		user_id, nullptr, &fopts
	};

	return user_room.get(std::nothrow, "ircd.read", room_id, [&closure]
	(const m::event &event)
	{
		const m::event::id &event_id
		{
			unquote(at<"content"_>(event).get("event_id"))
		};

		closure(event_id);
	});
}
