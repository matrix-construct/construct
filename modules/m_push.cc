// The Construct
//
// Copyright (C) The Construct Developers, Authors & Contributors
// Copyright (C) 2016-2020 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

namespace ircd::m::push
{
	static void execute(const event &, vm::eval &, const user::id &, const path &, const rule &, const event::idx &);
	static bool matching(const event &, vm::eval &, const user::id &, const path &, const rule &);
	static bool handle_kind(const event &, vm::eval &, const user::id &, const path &);
	static void handle_rules(const event &, vm::eval &, const user::id &, const string_view &scope);
	static void handle_event(const m::event &, vm::eval &);
	extern hookfn<vm::eval &> hook_event;
}

ircd::mapi::header
IRCD_MODULE
{
	"Matrix 13.13 :Push Notifications",
};

decltype(ircd::m::push::hook_event)
ircd::m::push::hook_event
{
	handle_event,
	{
		{ "_site", "vm.effect" },
	}
};

void
ircd::m::push::handle_event(const m::event &event,
                            vm::eval &eval)
try
{
	// No push notifications are generated from events in internal rooms.
	if(eval.room_internal)
		return;

	// No push notifications are generated from EDU's (at least directly).
	if(!event.event_id)
		return;

	const m::room::id &room_id
	{
		at<"room_id"_>(event)
	};

	const m::room::members members
	{
		room_id
	};

	members.for_each("join", my_host(), [&event, &eval]
	(const user::id &user_id, const event::idx &membership_event_idx)
	{
		// r0.6.0-13.13.15 Homeservers MUST NOT notify the Push Gateway for
		// events that the user has sent themselves.
		if(user_id == at<"sender"_>(event))
			return true;

		handle_rules(event, eval, user_id, "global");
		return true;
	});
}
catch(const ctx::interrupted &)
{
	throw;
}
catch(const std::exception &e)
{
	log::critical
	{
		log, "Push rule matching in %s :%s",
		string_view{event.event_id},
		e.what(),
	};
}

void
ircd::m::push::handle_rules(const event &event,
                            vm::eval &eval,
                            const user::id &user_id,
                            const string_view &scope)
{
	const push::path path[]
	{
		{ scope, "override",   string_view{}         },
		{ scope, "content",    string_view{}         },
		{ scope, "room",       at<"room_id"_>(event) },
		{ scope, "sender",     at<"sender"_>(event)  },
		{ scope, "underride",  string_view{}         },
	};

	for(const auto &p : path)
		if(!handle_kind(event, eval, user_id, p))
			break;
}

bool
ircd::m::push::handle_kind(const event &event,
                           vm::eval &eval,
                           const user::id &user_id,
                           const path &path)
{
	const user::pushrules pushrules
	{
		user_id
	};

	return pushrules.for_each(path, [&event, &eval, &user_id]
	(const auto &event_idx, const auto &path, const auto &rule)
	{
		if(matching(event, eval, user_id, path, rule))
		{
			execute(event, eval, user_id, path, rule, event_idx);
			return false; // false to break due to match
		}
		else return true;
	});
}

bool
ircd::m::push::matching(const event &event,
                        vm::eval &eval,
                        const user::id &user_id,
                        const path &path,
                        const rule &rule)
try
{
	const auto &[scope, kind, ruleid]
	{
		path
	};

	if(!json::get<"enabled"_>(rule))
		return false;

	push::match::opts opts;
	opts.user_id = user_id;
	const push::match match
	{
		event, rule, opts
	};

	#if 0
	log::debug
	{
		log, "event %s rule { %s, %s, %s } for %s %s",
		string_view{event.event_id},
		scope,
		kind,
		ruleid,
		string_view{user_id},
		bool(match)? "MATCH"_sv : string_view{}
	};
	#endif

	return bool(match);
}
catch(const ctx::interrupted &)
{
	throw;
}
catch(const std::exception &e)
{
	const auto &[scope, kind, ruleid]
	{
		path
	};

	log::error
	{
		log, "Push rule matching in %s for %s at { %s, %s, %s } :%s",
		string_view{event.event_id},
		string_view{user_id},
		scope,
		kind,
		ruleid,
		e.what(),
	};

	return false;
}

void
ircd::m::push::execute(const event &event,
                       vm::eval &eval,
                       const user::id &user_id,
                       const path &path,
                       const rule &rule,
                       const event::idx &rule_idx)
try
{
	const auto &[scope, kind, ruleid]
	{
		path
	};

	const json::array &actions
	{
		json::get<"actions"_>(rule)
	};

	log::debug
	{
		log, "event %s action { %s, %s, %s } for %s :%s",
		string_view{event.event_id},
		scope,
		kind,
		ruleid,
		string_view{user_id},
		string_view{json::get<"actions"_>(rule)},
	};

	// action is dont_notify or undefined etc
	if(!notifying(rule))
		return;

	user::notifications::opts opts;
	opts.room_id = eval.room_id;
	opts.only =
		highlighting(rule)?
			"highlight"_sv:
			string_view{};

	char type_buf[event::TYPE_MAX_SIZE];
	const auto &type
	{
		user::notifications::make_type(type_buf, opts)
	};

	const user::room user_room
	{
		user_id
	};

	send(user_room, at<"sender"_>(event), type, json::members
	{
		{ "event_idx",  long(eval.sequence)  },
		{ "rule_idx",   long(rule_idx)       },
		{ "user_id",    user_id              },
	});
}
catch(const ctx::interrupted &)
{
	throw;
}
catch(const std::exception &e)
{
	const auto &[scope, kind, ruleid]
	{
		path
	};

	log::error
	{
		log, "Push rule action in %s for %s at { %s, %s, %s } :%s",
		string_view{event.event_id},
		string_view{user_id},
		scope,
		kind,
		ruleid,
		e.what(),
	};
}
