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

static void handle_ircd_m_read(const m::event &, m::vm::eval &);
extern const m::hookfn<m::vm::eval &> _ircd_read_eval;

static void handle_implicit_receipt(const m::event &, m::vm::eval &);
extern const m::hookfn<m::vm::eval &> _implicit_receipt;

static void handle_m_receipt_m_read(const m::room::id &, const m::user::id &, const m::event::id &, const time_t &);
static void handle_m_receipt_m_read(const m::room::id &, const m::user::id &, const m::edu::m_receipt::m_read &);
static void handle_m_receipt_m_read(const m::event &, const m::room::id &, const json::object &);
static void handle_m_receipt(const m::event &, const m::room::id &, const json::object &);
static void handle_edu_m_receipt(const m::event &, m::vm::eval &);
extern const m::hookfn<m::vm::eval &> _m_receipt_eval;

mapi::header
IRCD_MODULE
{
	"Matrix Receipts"
};

log::log
receipt_log
{
	"matrix.receipt"
};

decltype(_m_receipt_eval)
_m_receipt_eval
{
	handle_edu_m_receipt,
	{
		{ "_site",   "vm.effect"  },
		{ "type",    "m.receipt"  },
	}
};

void
handle_edu_m_receipt(const m::event &event,
                     m::vm::eval &eval)
{
	if(json::get<"room_id"_>(event))
		return;

	const auto &origin
	{
		json::get<"origin"_>(event)
	};

	if(my_host(origin))
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

		handle_m_receipt(event, room_id, member.second);
	}
}

void
handle_m_receipt(const m::event &event,
                 const m::room::id &room_id,
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
			handle_m_receipt_m_read(event, room_id, member.second);
			continue;
		}

		log::dwarning
		{
			receipt_log, "Unhandled m.receipt type '%s' to room '%s'",
			type,
			string_view{room_id}
		};
	}
}

void
handle_m_receipt_m_read(const m::event &event,
                        const m::room::id &room_id,
                        const json::object &content)
{
	for(const auto &member : content)
	{
		const m::user::id user_id
		{
			unquote(member.first)
		};

		if(user_id.host() != json::get<"origin"_>(event))
		{
			log::dwarning
			{
				receipt_log, "ignoring m.receipt m.read from '%s' in %s for alien %s.",
				json::get<"origin"_>(event),
				string_view{room_id},
				string_view{user_id},
			};

			continue;
		}

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

	// This handler only cares about remote users.
	if(my(user))
		return;

	if(!exists(user))
	{
		log::dwarning
		{
			receipt_log, "ignoring m.receipt m.read for unknown %s in %s for %s",
			string_view{user_id},
			string_view{room_id},
			string_view{event_id}
		};

		return;
	}

	const auto evid
	{
		m::receipt::read(room_id, user_id, event_id, ts)
	};
}
catch(const std::exception &e)
{
	log::derror
	{
		receipt_log, "failed to save m.receipt m.read for %s in %s for %s :%s",
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
		receipt_log, "%s read by %s in %s @ %zd",
		string_view{event_id},
		string_view{user_id},
		string_view{room_id},
		ms
	};

	return evid;
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
		receipt_log, "Freshness of receipt in %s from %s for %s :%s",
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

decltype(_implicit_receipt)
_implicit_receipt
{
	handle_implicit_receipt,
	{
		{ "_site",   "vm.effect"       },
		{ "type",    "m.room.message"  },
		{ "origin",  my_host()         },
	}
};

/// This handler generates receipts for messages sent by that user. These are
/// required for notification counts. They're not broadcast, we just keep state
/// for them.
void
handle_implicit_receipt(const m::event &event,
                        m::vm::eval &eval)
try
{
	if(!json::get<"event_id"_>(event))
		return;

	const m::user::id &user_id
	{
		at<"sender"_>(event)
	};

	// This handler does not care about remote users.
	if(!my(user_id))
		return;

	const m::room::id &room_id
	{
		at<"room_id"_>(event)
	};

	const m::event::id &event_id
	{
		at<"event_id"_>(event)
	};

	const time_t &ms
	{
		at<"origin_server_ts"_>(event)
	};

	const auto receipt_event_id
	{
		m::receipt::read(room_id, user_id, event_id, ms)
	};
}
catch(const std::exception &e)
{
	log::error
	{
		receipt_log, "Implicit receipt hook for %s :%s",
		json::get<"event_id"_>(event),
		e.what(),
	};
}

decltype(_ircd_read_eval)
_ircd_read_eval
{
	handle_ircd_m_read,
	{
		{ "_site",   "vm.effect"  },
		{ "type",    "ircd.read"  },
		{ "origin",  my_host()    },
	}
};

/// This handler looks for ircd.read events and conducts a federation
/// broadcast of the m.receipt edu.
void
handle_ircd_m_read(const m::event &event,
                   m::vm::eval &eval)
try
{
	if(!json::get<"state_key"_>(event))
		return;

	// The state_key of an ircd.read event is the target room_id
	const m::room::id &room_id
	{
		at<"state_key"_>(event)
	};

	const m::user user
	{
		at<"sender"_>(event)
	};

	// This handler does federation broadcasts of receipts from
	// this server only.
	if(!my(user))
		return;

	const m::user::room user_room
	{
		at<"sender"_>(event)
	};

	// Ignore anybody that creates an ircd.read event in some other room.
	if(json::get<"room_id"_>(event) != user_room.room_id)
		return;

	const json::string &event_id
	{
		at<"content"_>(event).get("event_id")
	};

	// Lastly, we elide broadcasts of receipts for a user's own message.
	m::user::id::buf sender_buf;
	if(m::get(std::nothrow, event_id, "sender", sender_buf) == user.user_id)
		return;

	const time_t &ms
	{
		at<"content"_>(event).get<time_t>("ts", 0)
	};

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

	json::iov edu_event, content;
	const json::iov::push push[]
	{
		{ edu_event, { "type",     "m.receipt" } },
		{ edu_event, { "room_id",  room_id     } },
		{ content,   { room_id,
		{
			{ "m.read",
			{
				{ user.user_id, m_read }
			}}
		}}}
	};

	m::vm::copts opts;

	// EDU options
	opts.add_hash = false;
	opts.add_sig = false;
	opts.add_event_id = false;
	opts.add_origin = true;
	opts.add_origin_server_ts = false;
	opts.conforming = false;

	// Don't need to notify clients, the /sync system understood the
	// `ircd.read` directly. The federation sender is what we're hitting here.
	opts.notify_clients = false;

	m::vm::eval
	{
		edu_event, content, opts
	};
}
catch(const std::exception &e)
{
	log::error
	{
		receipt_log, "ircd.read hook on %s for federation broadcast :%s",
		json::get<"event_id"_>(event),
		e.what(),
	};
}
