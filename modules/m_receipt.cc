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

static void handle_ircd_read(const m::event &, m::vm::eval &);
extern m::hookfn<m::vm::eval &> _ircd_read_eval;

static void handle_implicit_receipt(const m::event &, m::vm::eval &);
extern m::hookfn<m::vm::eval &> _implicit_receipt;

static void handle_m_receipt_m_read(const m::room::id &, const m::user::id &, const m::event::id &, const json::object &data);
static void handle_m_receipt_m_read(const m::room::id &, const m::user::id &, const m::edu::m_receipt::m_read &);
static void handle_m_receipt_m_read(const m::event &, const m::room::id &, const json::object &);
static void handle_m_receipt(const m::event &, const m::room::id &, const json::object &);
static void handle_edu_m_receipt(const m::event &, m::vm::eval &);
extern m::hookfn<m::vm::eval &> _m_receipt_eval;

mapi::header
IRCD_MODULE
{
	"Matrix Receipts"
};

//
// EDU handler.
//

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

	if(my_host(json::get<"origin"_>(event)))
		return;

	const json::object &content
	{
		at<"content"_>(event)
	};

	for(const auto &[room_id, content] : content)
		handle_m_receipt(event, room_id, content);
}

void
handle_m_receipt(const m::event &event,
                 const m::room::id &room_id,
                 const json::object &content_)
{
	const bool relevant_room
	{
		m::local_joined(room_id)
	};

	if(!relevant_room)
	{
		log::dwarning
		{
			m::receipt::log, "Ignoring m.receipt from '%s' in %s :no local users joined.",
			json::get<"origin"_>(event),
			string_view{room_id},
		};

		return;
	}

	const bool access_allow
	{
		!m::room::server_acl::enable_write
		|| m::room::server_acl::check(room_id, json::get<"origin"_>(event))
	};

	if(!access_allow)
	{
		log::dwarning
		{
			m::receipt::log, "Ignoring m.receipt from '%s' in %s :denied by m.room.server_acl.",
			json::get<"origin"_>(event),
			string_view{room_id},
		};

		return;
	}

	for(const auto &[type, content] : content_)
	{
		if(type == "m.read")
		{
			handle_m_receipt_m_read(event, room_id, content);
			continue;
		}

		log::dwarning
		{
			m::receipt::log, "Unhandled m.receipt type '%s' to room '%s'",
			type,
			string_view{room_id}
		};
	}
}

void
handle_m_receipt_m_read(const m::event &event,
                        const m::room::id &room_id,
                        const json::object &content_)
{
	for(const auto &[user_id_, content] : content_)
	{
		const m::user::id user_id{user_id_};
		if(user_id.host() != json::get<"origin"_>(event))
		{
			log::dwarning
			{
				m::receipt::log, "Ignoring m.receipt m.read from '%s' in %s for alien %s.",
				json::get<"origin"_>(event),
				string_view{room_id},
				string_view{user_id},
			};

			continue;
		}

		handle_m_receipt_m_read(room_id, user_id, json::object{content});
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

	for(const json::string event_id : event_ids) try
	{
		handle_m_receipt_m_read(room_id, user_id, event_id, data);
	}
	catch(const std::exception &e)
	{
		log::derror
		{
			m::receipt::log, "Failed to handle m.receipt m.read for %s in %s for '%s' :%s",
			string_view{user_id},
			string_view{room_id},
			string_view{event_id},
			e.what()
		};
	}
}

void
handle_m_receipt_m_read(const m::room::id &room_id,
                        const m::user::id &user_id,
                        const m::event::id &event_id,
                        const json::object &data)
try
{
	const m::user user
	{
		user_id
	};

	// This handler only cares about remote users.
	if(my(user))
		return;

	// We used to ignore receipts from unknown users by returning from this
	// branch taken; this behavior was removed for both robustness of rooms
	// with incomplete state and for peeking.
	if(!exists(user))
		log::dwarning
		{
			m::receipt::log, "m.receipt m.read for unknown %s in %s for %s",
			string_view{user_id},
			string_view{room_id},
			string_view{event_id},
		};

	if(!exists(user))
		create(user);

	const auto evid
	{
		m::receipt::read(room_id, user_id, event_id, data)
	};
}
catch(const std::exception &e)
{
	log::derror
	{
		m::receipt::log, "Failed to save m.receipt m.read for %s in %s for %s :%s",
		string_view{user_id},
		string_view{room_id},
		string_view{event_id},
		e.what()
	};
}

//
// Internal -> Federation
//

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
	if(!event.event_id)
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
		event.event_id
	};

	char databuf[64];
	const json::object &data
	{
		json::stringify(mutable_buffer{databuf}, json::members
		{
			{ "ts", at<"origin_server_ts"_>(event) },
		})
	};

	const auto receipt_event_id
	{
		m::receipt::read(room_id, user_id, event_id, data)
	};
}
catch(const std::exception &e)
{
	log::error
	{
		m::receipt::log, "Implicit receipt hook for %s :%s",
		string_view{event.event_id},
		e.what(),
	};
}

decltype(_ircd_read_eval)
_ircd_read_eval
{
	handle_ircd_read,
	{
		{ "_site",   "vm.effect"  },
		{ "type",    "ircd.read"  },
		{ "origin",  my_host()    },
	}
};

/// This handler looks for ircd.read events and conducts a federation
/// broadcast of the m.receipt edu.
void
handle_ircd_read(const m::event &event,
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

	// Only broadcast if the user is joined to the room.
	//if(!membership(m::room(room_id), user, "join"))
	//	return;

	const m::user::room user_room
	{
		at<"sender"_>(event)
	};

	// Ignore anybody that creates an ircd.read event in some other room.
	if(json::get<"room_id"_>(event) != user_room.room_id)
		return;

	const json::string &event_id
	{
		at<"content"_>(event).at("event_id")
	};

	// MSC2285; if m.hidden is set here we don't broadcast this receipt
	// to the federation; nothing further to do here then.
	if(at<"content"_>(event).get("m.hidden", false))
		return;

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

	// EDU options
	m::vm::copts opts;
	opts.edu = true;
	opts.prop_mask.reset();
	opts.prop_mask.set("origin");

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
		m::receipt::log, "ircd.read hook on %s for federation broadcast :%s",
		string_view{event.event_id},
		e.what(),
	};
}
