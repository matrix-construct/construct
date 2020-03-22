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
	static void handle_action(const event &, vm::eval &, const match::opts &, const path &, const rule &, const string_view &action);
	static bool handle_rule(const event &, vm::eval &, const match::opts &, const path &, const rule &);
	static bool handle_kind(const event &, vm::eval &, const user::id &, const path &);
	static void handle_rules(const event &, vm::eval &, const user::id &);
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
		handle_rules(event, eval, user_id);
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
                            const user::id &user_id)
{
	const push::path path[]
	{
		{ "global",  "override",   string_view{}         },
		{ "global",  "content",    string_view{}         },
		{ "global",  "room",       at<"room_id"_>(event) },
		{ "global",  "sender",     at<"sender"_>(event)  },
		{ "global",  "underride",  string_view{}         },
	};

	for(const auto &p : path)
		handle_kind(event, eval, user_id, p);
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

	push::match::opts opts;
	opts.user_id = user_id;
	pushrules.for_each(path, [&event, &eval, &opts]
	(const push::path &path, const push::rule &rule)
	{
		handle_rule(event, eval, opts, path, rule);
		return true;
	});

	return true;
}

bool
ircd::m::push::handle_rule(const event &event,
                           vm::eval &eval,
                           const match::opts &opts,
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

	const push::match match
	{
		event, rule, opts
	};

	log::debug
	{
		log, "event %s rule {%s, %s, %s} for %s %s",
		string_view{event.event_id},
		scope,
		kind,
		ruleid,
		string_view{opts.user_id},
		bool(match)? "MATCH"_sv : "-----"_sv,
	};

	if(!bool(match))
		return false;

	const json::array &actions
	{
		json::get<"actions"_>(rule)
	};

	for(const string_view &action : actions)
		handle_action(event, eval, opts, path, rule, action);

	return true;
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
		log, "Push rule matching in %s for %s at {%s, %s, %s} :%s",
		string_view{event.event_id},
		string_view{opts.user_id},
		scope,
		kind,
		ruleid,
		e.what(),
	};

	return false;
}

void
ircd::m::push::handle_action(const event &event,
                             vm::eval &eval,
                             const match::opts &opts,
                             const path &path,
                             const rule &rule,
                             const string_view &action)
try
{
	const auto &[scope, kind, ruleid]
	{
		path
	};

	log::debug
	{
		log, "event %s rule {%s, %s, %s} for %s action :%s",
		string_view{event.event_id},
		scope,
		kind,
		ruleid,
		string_view{opts.user_id},
		action,
	};


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
		log, "Push rule action in %s for %s at {%s, %s, %s} :%s",
		string_view{event.event_id},
		string_view{opts.user_id},
		scope,
		kind,
		ruleid,
		e.what(),
	};
}
