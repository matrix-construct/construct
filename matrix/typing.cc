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

log::log
typing_log
{
	"m.typing"
};

struct typist
{
	using is_transparent = void;

	system_point timesout;
	m::user::id::buf user_id;
	m::room::id::buf room_id;

	bool operator()(const typist &a, const string_view &b) const;
	bool operator()(const string_view &a, const typist &b) const;
	bool operator()(const typist &a, const typist &b) const;
};

ctx::dock
timeout_dock;

std::set<typist, typist>
typists;

conf::item<milliseconds>
timeout_max
{
	{ "name",     "ircd.typing.timeout.max" },
	{ "default",  90 * 1000L                },
};

conf::item<milliseconds>
timeout_min
{
	{ "name",     "ircd.typing.timeout.min" },
	{ "default",  15 * 1000L                },
};

conf::item<milliseconds>
timeout_int
{
	{ "name",     "ircd.typing.timeout.int" },
	{ "default",  5 * 1000L                 },
};

static system_point calc_timesout(milliseconds relative);
static bool update_state(const m::edu::m_typing &);

//
// typing edu handler stack (local and remote)
//

static m::event::id::buf set_typing(const m::edu::m_typing &edu);
static void _handle_edu_m_typing(const m::event &, const m::typing &edu);
static void handle_edu_m_typing(const m::event &, m::vm::eval &);

/// Hooks all federation typing edus from remote servers as well as
/// the above commit from local clients. This hook rewrites the edu into
/// a new event formatted for client /sync and then runs that through eval
/// so our clients can receive the typing events.
///
m::hookfn<m::vm::eval &>
_m_typing_eval
{
	handle_edu_m_typing,
	{
		{ "_site",   "vm.eval"     },
		{ "type",    "m.typing"    },
	}
};

void
handle_edu_m_typing(const m::event &event,
                    m::vm::eval &eval)
{
	const json::object &content
	{
		at<"content"_>(event)
	};

	_handle_edu_m_typing(event, content);
}

void
_handle_edu_m_typing(const m::event &event,
                     const m::typing &edu)
{
	// This check prevents interference between the two competing edu formats;
	// The federation edu has a room_id field while the client edu only has a
	// user_id's array. We don't need to hook on the client edu here.
	if(!json::get<"room_id"_>(edu))
		return;

	const auto &origin
	{
		at<"origin"_>(event)
	};

	const m::room::id &room_id
	{
		at<"room_id"_>(edu)
	};

	const m::user::id &user_id
	{
		at<"user_id"_>(edu)
	};

	// Check if this server can send an edu for this user. We make an exception
	// for our server to allow the timeout worker to use this codepath.
	if(!my_host(origin))
		if(user_id.host() != origin)
		{
			log::dwarning
			{
				typing_log, "Ignoring %s from %s for alien %s",
				at<"type"_>(event),
				origin,
				string_view{user_id}
			};

			return;
		}

	// Check if this server can write to the room based on the m.room.server_acl.
	if(!my_host(origin))
		if(m::room::server_acl::enable_write && !m::room::server_acl::check(room_id, origin))
		{
			log::dwarning
			{
				typing_log, "Ignoring %s from '%s' in %s :denied by m.room.server_acl.",
				at<"type"_>(event),
				origin,
				string_view{room_id},
			};

			return;
		}

	// Update the typing state map for edu's from other servers only; the
	// state map was already updated for our clients in the committer. Also
	// condition for skipping redundant updates here too based on the state.
	if(!my_host(origin))
	{
		// Check if the user is actually in the room. The check is in this
		// branch for remote servers only because our committer above did this
		// already for our client.
		const m::room room{room_id};
		if(!membership(room, user_id, "join"))
		{
			log::dwarning
			{
				typing_log, "Ignoring %s from %s for user %s because not in room '%s'",
				at<"type"_>(event),
				origin,
				string_view{user_id},
				string_view{room_id},
			};

			return;
		}

		// Set the (non-spec) timeout field of the edu which remote servers
		// don't/can't set and then update the state. Use the maximum timeout
		// value here because the minimum might unfairly time them out.
		auto _edu(edu);
		const milliseconds max(timeout_max), min(timeout_min);
		json::get<"timeout"_>(_edu) = std::clamp(json::get<"timeout"_>(_edu), min.count(), max.count());
		if(!update_state(_edu))
			return;
	}

	set_typing(edu);
}

//
// interface
//

/// typing commit handler stack (local user)
///
/// This function is called via ircd::m::typing linkage to create a typing
/// event originating from our client. This event takes the form of the
/// federation edu and is broadcast to servers. Unfortunately the matrix
/// client spec has a different edu format for typing; so to propagate this
/// event to clients we hook it during eval and create a new event formatted
/// for clients and then run that through eval too. (see below).
///
ircd::m::typing::commit::commit(const m::typing &edu)
{
	using json::at;

	// Check if the user is actually in the room.
	const m::room room
	{
		at<"room_id"_>(edu)
	};

	// Only allow user to send typing events to rooms they are joined...
	if(!membership(room, at<"user_id"_>(edu), "join"))
		throw m::FORBIDDEN
		{
			"Cannot type in a room %s to which you are not joined",
			string_view{room.room_id}
		};

	// If the user does not want to transmit typing events to this room,
	// bail out here.
	if(!allow(at<"user_id"_>(edu), room, "send"))
		return;

	// Clients like Riot will send erroneous and/or redundant typing requests
	// for example requesting typing=false when the state already =false.
	// We don't want to tax the vm::eval for this noise so we try to update
	// state first and if that returns false it indicates we should ignore.
	if(!update_state(edu))
		return;

	json::iov event, content;
	const json::iov::push push[]
	{
		{ event,    { "type",     "m.typing"                  } },
		{ event,    { "room_id",  at<"room_id"_>(edu)         } },
		{ content,  { "user_id",  at<"user_id"_>(edu)         } },
		{ content,  { "room_id",  at<"room_id"_>(edu)         } },
		{ content,  { "typing",   json::get<"typing"_>(edu)   } },
	};

	m::vm::copts opts;
	opts.edu = true;
	opts.prop_mask.reset();
	opts.prop_mask.set("origin");
	m::vm::eval
	{
		event, content, opts
	};
}

bool
ircd::m::typing::allow(const user::id &user_id,
                       const room::id &room_id,
                       const string_view &allow_type)
{
	const user::room user_room
	{
		user_id
	};

	const room::state state
	{
		user_room
	};

	char buf[event::TYPE_MAX_SIZE+1]
	{
		"ircd.typing.disable."
	};

	const string_view &type
	{
		strlcat{buf, allow_type}
	};

	const string_view &state_key
	{
		room_id
	};

	return !state.has(type, state_key);
}

bool
ircd::m::typing::for_each(const closure &closure)
{
	// User cannot yield in their closure because the iteration
	// may be invalidated by the timeout worker during their yield.
	const ctx::critical_assertion ca;

	for(const auto &t : typists)
	{
		const time_t timeout
		{
			system_clock::to_time_t(t.timesout)
		};

		const m::typing event
		{
			{ "user_id",  t.user_id   },
			{ "room_id",  t.room_id   },
			{ "typing",   true        },
			{ "timeout",  timeout     },
		};

		if(!closure(event))
			return false;
	}

	return true;
}

//
// timeout worker stack
//

static void timeout_timeout(const typist &);
static bool timeout_check();
static void timeout_worker();
static context timeout_context
{
	"typing",
	256_KiB,
	context::POST,
	timeout_worker
};

static const ircd::run::changed
timeout_context_terminate
{
	run::level::QUIT, []
	{
		timeout_context.terminate();
	}
};

void
__attribute__((noreturn))
timeout_worker()
{
	while(1)
	{
		timeout_dock.wait([]
		{
			return !typists.empty();
		});

		if(!timeout_check())
			ctx::sleep(milliseconds(timeout_int));
	}
}

bool
timeout_check()
try
{
	const auto now
	{
		ircd::now<system_point>()
	};

	for(auto it(begin(typists)); it != end(typists); ++it)
		if(it->timesout < now)
		{
			// have to restart the loop if there's a timeout because
			// the call will have yields and invalidate iterators etc.
			const auto copy(*it);
			typists.erase(it);
			timeout_timeout(copy);
			return true;
		}

	return false;
}
catch(const std::exception &e)
{
	log::critical
	{
		typing_log, "Typing timeout check :%s",
		e.what(),
	};

	return false;
}

void
timeout_timeout(const typist &t)
try
{
	assert(run::level == run::level::RUN);

	const m::typing edu
	{
		{ "user_id",  t.user_id   },
		{ "room_id",  t.room_id   },
		{ "typing",   false       },
	};

	log::debug
	{
		typing_log, "Typing timeout for %s in %s",
		string_view{t.user_id},
		string_view{t.room_id}
	};

	m::event event;
	json::get<"origin"_>(event) = my_host();
	json::get<"type"_>(event) = "m.typing"_sv;

	// Call this manually because it currently composes the event
	// sent to clients to stop the typing for this timed out user.
	_handle_edu_m_typing(event, edu);
}
catch(const std::exception &e)
{
	log::error
	{
		typing_log, "Typing timeout for %s in %s :%s",
		string_view{t.user_id},
		string_view{t.room_id},
		e.what(),
	};
}

//
// internal
//

ircd::m::event::id::buf
set_typing(const m::edu::m_typing &edu)
{
	assert(json::get<"room_id"_>(edu));
	const m::user::id &user_id
	{
		at<"user_id"_>(edu)
	};

	const m::user::room user_room
	{
		user_id
	};

	const auto &timeout
	{
		json::get<"timeout"_>(edu)?
			json::get<"timeout"_>(edu):
		json::get<"typing"_>(edu)?
			milliseconds(timeout_max).count():
			0L
	};

	const auto evid
	{
		send(user_room, user_id, "ircd.typing",
		{
			{ "room_id",  at<"room_id"_>(edu)         },
			{ "typing",   json::get<"typing"_>(edu)   },
			{ "timeout",  timeout                     },
		})
	};

	char pbuf[32];
	log::info
	{
		typing_log, "%s %s typing in %s timeout:%s",
		string_view{user_id},
		json::get<"typing"_>(edu)?
			"started"_sv:
			"stopped"_sv,
		at<"room_id"_>(edu),
		pretty(pbuf, milliseconds(long(timeout)), 1),
	};

	return evid;
}

bool
update_state(const m::edu::m_typing &object)
try
{
	const auto &user_id
	{
		at<"user_id"_>(object)
	};

	const auto &room_id
	{
		at<"room_id"_>(object)
	};

	const auto &typing
	{
		at<"typing"_>(object)
	};

	const milliseconds timeout
	{
		at<"timeout"_>(object)
	};

	auto it
	{
		typists.lower_bound(user_id)
	};

	const bool was_typing
	{
		it != end(typists) && it->user_id == user_id
	};

	if(typing && !was_typing)
	{
		typists.emplace_hint(it, typist
		{
			calc_timesout(timeout), user_id, room_id
		});

		timeout_dock.notify_one();
	}
	else if(typing && was_typing)
	{
		auto &t(const_cast<typist &>(*it));
		t.timesout = calc_timesout(timeout);
	}
	else if(!typing && was_typing)
	{
		typists.erase(it);
	}

	const bool transmit
	{
		(typing && !was_typing) || (!typing && was_typing)
	};

	log::debug
	{
		typing_log, "Typing %s in %s now[%b] was[%b] xmit[%b]",
		string_view{at<"user_id"_>(object)},
		string_view{at<"room_id"_>(object)},
		json::get<"typing"_>(object),
		was_typing,
		transmit
	};

	return transmit;
}
catch(const std::exception &e)
{
	log::error
	{
		typing_log, "Failed to update state :%s",
		e.what(),
	};

	throw;
}

system_point
calc_timesout(milliseconds timeout)
{
	timeout = std::max(timeout, milliseconds(timeout_min));
	timeout = std::min(timeout, milliseconds(timeout_max));
	return now<system_point>() + timeout;
}

//
// typist struct
//

bool
typist::operator()(const typist &a, const string_view &b)
const
{
    return a.user_id < b;
}

bool
typist::operator()(const string_view &a, const typist &b)
const
{
    return a < b.user_id;
}

bool
typist::operator()(const typist &a, const typist &b)
const
{
    return a.user_id < b.user_id;
}
