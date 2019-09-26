// Matrix Construct
//
// Copyright (C) Matrix Construct Developers, Authors & Contributors
// Copyright (C) 2016-2019 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

using namespace ircd;

static void handle_m_ignored_user(const m::event &, m::vm::eval &, const m::user::id &, const json::object &);
static void handle_m_ignored_user_list(const m::event &, m::vm::eval &eval);

mapi::header
IRCD_MODULE
{
	"14.24 :Ignoring Users"
};

/*
m::hookfn<m::vm::eval &>
_m_ignored_user_list
{
	handle_m_ignored_user_list,
	{
		{ "_site",      "vm.eval"              },
		{ "type",       "ircd.account_data"    },
		{ "state_key",  "m.ignored_user_list"  },
		{ "origin",      my_host()             },
	}
};
*/

void
handle_m_ignored_user_list(const m::event &event,
                           m::vm::eval &eval)
try
{
	const auto &sender{json::get<"sender"_>(event)};
	const auto &room_id{json::get<"room_id"_>(event)};
	const m::user::room user_room{sender};
	if(user_room.room_id != room_id)
		return;

	const json::object &content
	{
		json::get<"content"_>(event)
	};

	const json::object &ignored_users
	{
		content.get("ignored_users")
	};

	for(const auto &[user_id, object] : ignored_users)
		handle_m_ignored_user(event, eval, user_id, object);
}
catch(const std::exception &e)
{
	log::derror
	{
		m::log, "m.ignored_user_list from %s :%s",
		json::get<"sender"_>(event),
		e.what(),
	};

	throw;
}

void
handle_m_ignored_user(const m::event &event,
                      m::vm::eval &eval,
                      const m::user::id &user_id,
                      const json::object &object)
{
	log::debug
	{
		m::log, "%s is now ignoring %s",
		string_view{at<"sender"_>(event)},
		string_view{user_id},
	};
}
