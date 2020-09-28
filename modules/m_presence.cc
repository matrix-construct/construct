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

static void handle_ircd_presence(const m::event &, m::vm::eval &);
static void handle_edu_m_presence_object(const m::event &, const m::presence &edu);
static void handle_edu_m_presence(const m::event &, m::vm::eval &);

mapi::header
IRCD_MODULE
{
	"Matrix Presence"
};

/// Coarse enabler for incoming federation presence events. If this is
/// disabled then all presence coming over the federation is ignored. Note
/// that there are other ways to degrade or ignore presence in various
/// other subsystems like sync without losing the data; however presence
/// data over the federation is considerable and tiny deployments which
/// won't /sync presence to clients should probably quench it here too.
conf::item<bool>
federation_incoming
{
	{ "name",     "ircd.m.presence.federation.incoming" },
	{ "default",  true                                  },
};

/// Coarse enabler to send presence events over the federation.
conf::item<bool>
federation_send
{
	{ "name",     "ircd.m.presence.federation.send" },
	{ "default",  false                             },
};

/// Minimum time between presence events for the same user
conf::item<seconds>
federation_rate_user
{
	{ "name",     "ircd.m.presence.federation.rate.user" },
	{ "default",  305L                                   },
};

log::log
presence_log
{
	"m.presence"
};

/// This hook processes incoming m.presence events from the federation and
/// turns them into ircd.presence events in the user's room.
const m::hookfn<m::vm::eval &>
_m_presence_eval
{
	handle_edu_m_presence,
	{
		{ "_site",   "vm.eval"     },
		{ "type",    "m.presence"  },
	}
};

extern const string_view
valid_states[];

void
handle_edu_m_presence(const m::event &event,
                      m::vm::eval &eval)
try
{
	if(!federation_incoming)
		return;

	if(my(event))
		return;

	const json::object &content
	{
		at<"content"_>(event)
	};

	const json::array &push
	{
		content.get("push")
	};

	for(const json::object presence : push)
		handle_edu_m_presence_object(event, presence);
}
catch(const std::exception &e)
{
	log::derror
	{
		presence_log, "Presence from %s :%s",
		json::get<"origin"_>(event),
		e.what(),
	};
}

void
handle_edu_m_presence_object(const m::event &event,
                             const m::presence &object)
try
{
	const m::user::id &user_id
	{
		at<"user_id"_>(object)
	};

	if(user_id.host() != at<"origin"_>(event))
	{
		log::dwarning
		{
			presence_log, "Ignoring %s from %s for user %s",
			at<"type"_>(event),
			at<"origin"_>(event),
			string_view{user_id}
		};

		return;
	}

	bool useful{true};
	const auto closure{[&event, &object, &useful]
	(const m::event &existing_event)
	{
		const json::object &existing_object
		{
			json::get<"content"_>(existing_event)
		};

		// This check shouldn't have to exist. I think this was a result of corrupting
		// my DB during development and it fails on only one user. Nevertheless, it's
		// a valid assertion so might as well keep it.
		if(unlikely(json::get<"user_id"_>(object) != unquote(existing_object.get("user_id"))))
		{
			//log::critical("%s != %s", json::get<"user_id"_>(object), unquote(existing_object.get("user_id")));
			return;
		}

		assert(json::get<"user_id"_>(object) == unquote(existing_object.get("user_id")));

		const auto &prev_active_ago
		{
			existing_object.get<time_t>("last_active_ago")
		};

		const time_t &now_active_ago
		{
			json::get<"last_active_ago"_>(object)
		};

		const time_t &prev_active_absolute
		{
			json::get<"origin_server_ts"_>(existing_event) - prev_active_ago
		};

		const time_t &now_active_absolute
		{
			json::get<"origin_server_ts"_>(event) - now_active_ago
		};

		const time_t ts_diff
		{
			ircd::time<milliseconds>() - json::get<"origin_server_ts"_>(existing_event)
		};

		// Ignore any spam on a per-user basis here.
		if(seconds(ts_diff / 1000) < seconds(federation_rate_user))
			useful = false;

		// First way to filter out the synapse presence spam bug is seeing
		// if the update is older than the last update.
		else if(now_active_absolute < prev_active_absolute)
			useful = false;

		else if(json::get<"presence"_>(object) != unquote(existing_object.get("presence")))
			useful = true;

		else if(json::get<"currently_active"_>(object) != existing_object.get<bool>("currently_active"))
			useful = true;

		else if(json::get<"currently_active"_>(object))
			useful = true;

		else
			useful = false;
	}};

	static const m::event::fetch::opts fopts
	{
		m::event::keys::include {"content", "origin_server_ts"}
	};

	m::presence::get(std::nothrow, user_id, closure, &fopts);

	if(!useful)
	{
		log::dwarning
		{
			presence_log, "presence spam from %s %s is %s and %s %zd seconds ago",
			at<"origin"_>(event),
			string_view{user_id},
			json::get<"currently_active"_>(object)? "active"_sv : "inactive"_sv,
			json::get<"presence"_>(object),
			json::get<"last_active_ago"_>(object) / 1000L
		};

		return;
	}

	const auto evid
	{
		m::presence::set(object)
	};

	log::info
	{
		presence_log, "%s %s is %s and %s %zd seconds ago",
		at<"origin"_>(event),
		string_view{user_id},
		json::get<"currently_active"_>(object)? "active"_sv : "inactive"_sv,
		json::get<"presence"_>(object),
		json::get<"last_active_ago"_>(object) / 1000L
	};
}
catch(const m::error &e)
{
	log::error
	{
		presence_log, "Presence from %s :%s :%s",
		json::get<"origin"_>(event),
		e.what(),
		e.content
	};
}

/// This hook processes ircd.presence events generated internally from local
/// users and converts them to m.presence over the federation.
const m::hookfn<m::vm::eval &>
_ircd_presence_eval
{
	handle_ircd_presence,
	{
		{ "_site",   "vm.effect"      },
		{ "type",    "ircd.presence"  },
	}
};

void
handle_ircd_presence(const m::event &event,
                     m::vm::eval &eval)
try
{
	if(!federation_send)
		return;

	const m::user::id &user_id
	{
		json::get<"sender"_>(event)
	};

	if(!my(user_id))
		return;

	// The event has to be an ircd.presence in the user's room, not just a
	// random ircd.presence typed event in some other room...
	if(!m::user::room::is(json::get<"room_id"_>(event), user_id))
		return;

	// Get the spec EDU data from our PDU's content
	const m::edu::m_presence edu
	{
		json::get<"content"_>(event)
	};

	// Check if the user_id in the content is legitimate. This should have
	// been checked on any input side, but nevertheless we'll ignore any
	// discrepancies here for now.
	if(unlikely(json::get<"user_id"_>(edu) != user_id))
		return;

	// The matrix EDU format requires us to wrap this data in an array
	// called "push" so we copy content into this stack buffer :/
	char buf[512];
	json::stack out{buf};
	{
		json::stack::array push{out};
		push.append(edu);
	}

	// Note that "sender" is intercepted by the federation sender and not
	// actually sent over the wire.
	json::iov edu_event, content;
	const json::iov::push pushed[]
	{
		{ edu_event,  { "type",    "m.presence"     }},
		{ edu_event,  { "sender",  user_id          }},
		{ content,    { "push",    out.completed()  }},
	};

	// Setup for a core injection of an EDU.
	m::vm::copts opts;
	opts.edu = true;
	opts.prop_mask.reset();            // Clear all PDU properties
	opts.prop_mask.set("origin");
	opts.notify_clients = false;       // Client /sync already saw the ircd.presence

	// Execute
	m::vm::eval
	{
		edu_event, content, opts
	};

	log::info
	{
		presence_log, "%s is %s and %s %zd seconds ago",
		string_view{user_id},
		json::get<"currently_active"_>(edu)? "active"_sv : "inactive"_sv,
		json::get<"presence"_>(edu),
		json::get<"last_active_ago"_>(edu) / 1000L,
	};
}
catch(const std::exception &e)
{
	log::error
	{
		presence_log, "Presence from our %s to federation :%s",
		string_view{json::get<"sender"_>(event)},
		e.what(),
	};
}
