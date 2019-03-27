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

static void handle_m_receipt_m_read(const m::room::id &, const m::user::id &, const m::event::id &, const time_t &);
static void handle_m_receipt_m_read(const m::room::id &, const m::user::id &, const m::edu::m_receipt::m_read &);
static void handle_m_receipt_m_read(const m::room::id &, const json::object &);
static void handle_m_receipt(const m::room::id &, const json::object &);
static void handle_edu_m_receipt(const m::event &, m::vm::eval &);

mapi::header
IRCD_MODULE
{
	"Matrix Receipts"
};

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
			m::log, "Unhandled m.receipt type '%s' to room '%s'",
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
			m::log, "ignoring m.receipt m.read for unknown %s in %s for %s",
			string_view{user_id},
			string_view{room_id},
			string_view{event_id}
		};

		return;
	}

	m::vm::copts copts;
	copts.notify_clients = false;
	const m::user::room user_room
	{
		user, &copts
	};

	const auto evid
	{
		send(user_room, user_id, "ircd.read", room_id,
		{
			{ "event_id", event_id },
			{ "ts",       ts       }
		})
	};

	log::info
	{
		m::log, "%s read by %s in %s @ %zd",
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
		m::log, "failed to save m.receipt m.read for %s in %s for %s :%s",
		string_view{user_id},
		string_view{room_id},
		string_view{event_id},
		e.what()
	};
}

m::event::id::buf
IRCD_MODULE_EXPORT
ircd::m::receipt::read(const m::room::id &room_id,
                       const m::user::id &user_id,
                       const m::event::id &event_id,
                       const time_t &ms)
{
	const m::user::room user_room
	{
		user_id
	};

	const auto evid
	{
		send(user_room, user_id, "ircd.read", room_id,
		{
			{ "event_id", event_id },
			{ "ts",       ms       }
		})
	};

	log::info
	{
		m::log, "%s read by %s in %s @ %zd => %s (local)",
		string_view{event_id},
		string_view{user_id},
		string_view{room_id},
		ms,
		string_view{evid}
	};

	//
	// federation send composition
	//

	const json::value event_ids[]
	{
		{ event_id }
	};

	const json::members m_read
	{
		{ "data",
		{
			{ "ts", ms }
		}},
		{ "event_ids",
		{
			event_ids, 1
		}},
	};

	json::iov event, content;
	const json::iov::push push[]
	{
		{ event,    { "type",     "m.receipt" } },
		{ event,    { "room_id",  room_id     } },
		{ content,  { room_id,
		{
			{ "m.read",
			{
				{ user_id, m_read }
			}}
		}}}
	};

	m::vm::copts opts;
	opts.add_hash = false;
	opts.add_sig = false;
	opts.add_event_id = false;
	opts.add_origin = true;
	opts.add_origin_server_ts = false;
	opts.conforming = false;
	return m::vm::eval
	{
		event, content, opts
	};
}

bool
IRCD_MODULE_EXPORT
ircd::m::receipt::read(const m::room::id &room_id,
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


/// Does the user wish to not send receipts for events sent by its specific
/// sender?
bool
IRCD_MODULE_EXPORT
ircd::m::receipt::ignoring(const m::user &user,
                           const m::event::id &event_id)
{
	bool ret{false};
	m::get(std::nothrow, event_id, "sender", [&ret, &user]
	(const string_view &sender)
	{
		const m::user::room user_room{user};
		ret = user_room.has("ircd.read.ignore", sender);
	});

	return ret;
}

/// Does the user wish to not send receipts for events for this entire room?
bool
IRCD_MODULE_EXPORT
ircd::m::receipt::ignoring(const m::user &user,
                           const m::room::id &room_id)
{
	const m::user::room user_room{user};
	return user_room.has("ircd.read.ignore", room_id);
}

bool
IRCD_MODULE_EXPORT
ircd::m::receipt::freshest(const m::room::id &room_id,
                           const m::user::id &user_id,
                           const m::event::id &event_id)
try
{
	const m::user::room user_room
	{
		user_id
	};

	bool ret{true};
	user_room.get("ircd.read", room_id, [&ret, &event_id]
	(const m::event &event)
	{
		const auto &content
		{
			at<"content"_>(event)
		};

		const m::event::id &previous_id
		{
			unquote(content.get("event_id"))
		};

		if(event_id == previous_id)
		{
			ret = false;
			return;
		}

		const m::event::idx &previous_idx
		{
			index(previous_id)
		};

		const m::event::idx &event_idx
		{
			index(event_id)
		};

		ret = event_idx > previous_idx;
	});

	return ret;
}
catch(const std::exception &e)
{
	log::derror
	{
		m::log, "Freshness of receipt in %s from %s for %s :%s",
		string_view{room_id},
		string_view{user_id},
		string_view{event_id},
		e.what()
	};

	return true;
}

bool
IRCD_MODULE_EXPORT
ircd::m::receipt::exists(const m::room::id &room_id,
                         const m::user::id &user_id,
                         const m::event::id &event_id)
{
	const m::user::room user_room
	{
		user_id
	};

	bool ret{false};
	user_room.get(std::nothrow, "ircd.read", room_id, [&ret, &event_id]
	(const m::event &event)
	{
		const auto &content
		{
			at<"content"_>(event)
		};

		ret = unquote(content.get("event_id")) == event_id;
	});

	return ret;
}
