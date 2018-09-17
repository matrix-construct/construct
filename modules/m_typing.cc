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
	"Matrix Typing"
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

ctx::dock timeout_dock;
std::set<typist, typist> typists;

static system_point calc_timesout(milliseconds relative);
static bool update_state(const m::typing &);
extern "C" m::event::id::buf commit(const m::typing &edu);
extern "C" bool for_each(const m::typing::closure_bool &);

//
// typing commit handler stack (local user)
//

// This function is called via ircd::m::typing linkage to create a typing
// event originating from our client. This event takes the form of the
// federation edu and is broadcast to servers. Unfortunately the matrix
// client spec has a different edu format for typing; so to propagate this
// event to clients we hook it during eval and create a new event formatted
// for clients and then run that through eval too. (see below).
//
m::event::id::buf
commit(const m::typing &edu)
{
	// Clients like Riot will send erroneous and/or redundant typing requests
	// for example requesting typing=false when the state already =false.
	// We don't want to tax the vm::eval for this noise so we try to update
	// state first and if that returns false it indicates we should ignore.
	if(!update_state(edu))
		return {};

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
	opts.add_hash = false;
	opts.add_sig = false;
	opts.add_event_id = false;
	opts.add_origin = true;
	opts.add_origin_server_ts = false;
	opts.conforming = false;

	// Because the matrix spec should use the same format for client
	// and federation typing events, we don't prevent this from being
	// sent to clients who wish to preemptively implement this format.
	//opts.notify_clients = false;

	return m::vm::eval
	{
		event, content, opts
	};
}

//
// typing edu handler stack (local and remote)
//

static void _handle_edu_m_typing(const m::event &, const m::typing &edu);
static void _handle_edu_m_typing(const m::event &, const m::typing &edu);
static void handle_edu_m_typing(const m::event &, m::vm::eval &);

/// Hooks all federation typing edus from remote servers as well as
/// the above commit from local clients. This hook rewrites the edu into
/// a new event formatted for client /sync and then runs that through eval
/// so our clients can receive the typing events.
///
const m::hookfn<m::vm::eval &>
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

	const m::room::id &room_id
	{
		at<"room_id"_>(edu)
	};

	const m::user::id &user_id
	{
		at<"user_id"_>(edu)
	};

	if(user_id.host() != at<"origin"_>(event))
	{
		log::dwarning
		{
			"Ignoring %s from %s for user %s",
			at<"type"_>(event),
			at<"origin"_>(event),
			string_view{user_id}
		};

		return;
	}

	log::info
	{
		"%s %s %s typing in %s",
		at<"origin"_>(event),
		string_view{user_id},
		json::get<"typing"_>(edu)? "started"_sv : "stopped"_sv,
		string_view{room_id}
	};

	m::event typing{event};
	json::get<"room_id"_>(typing) = room_id;
	json::get<"type"_>(typing) = "m.typing"_sv;

	// Buffer has to hold user mxid and then some JSON overhead (see below)
	char buf[m::id::MAX_SIZE + 65];
	const json::value user_ids[]
	{
		{ user_id }
	};

	json::get<"content"_>(typing) = json::stringify(mutable_buffer{buf}, json::members
	{
		{ "user_ids", { user_ids, size_t(bool(json::get<"typing"_>(edu))) } }
	});

	m::vm::opts vmopts;
	vmopts.notify_servers = false;
	vmopts.conforming = false;
	m::vm::eval eval
	{
		typing, vmopts
	};
}

//
// timeout worker stack
//

static void timeout_timeout(const typist &);
static bool timeout_check();
static void timeout_worker();
context timeout_context
{
	"typing", 128_KiB, context::POST, timeout_worker
};

void
timeout_worker()
{
	while(1)
	{
		timeout_dock.wait([]
		{
			return !typists.empty();
		});

		if(!timeout_check())
			ctx::sleep(seconds(5));
	}
}

bool
timeout_check()
{
	const auto now
	{
		ircd::now<system_point>()
	};

	auto it(begin(typists));
	for(auto it(begin(typists)); it != end(typists); ++it)
		if(it->timesout < now)
		{
			// have to restart the loop if there's a timeout because
			// the call will update typist state and invalidate iterators.
			timeout_timeout(*it);
			return true;
		}

	return false;
}

void
timeout_timeout(const typist &t)
{
	const m::typing event
	{
		{ "user_id",  t.user_id   },
		{ "room_id",  t.room_id   },
		{ "typing",   false       },
	};

	log::debug
	{
		"Typing timeout for %s in %s",
		string_view{t.user_id},
		string_view{t.room_id}
	};

	m::typing::commit
	{
		event
	};
}

//
// misc
//

bool
for_each(const m::typing::closure_bool &closure)
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

bool
update_state(const m::typing &object)
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
		"Typing %s in %s now[%b] was[%b] xmit[%b]",
		string_view{at<"user_id"_>(object)},
		string_view{at<"room_id"_>(object)},
		json::get<"typing"_>(object),
		was_typing,
		transmit
	};

	return transmit;
}

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
