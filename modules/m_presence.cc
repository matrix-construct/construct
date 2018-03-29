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
	"Matrix Presence"
};

const string_view
valid_states[]
{
	"online", "offline", "unavailable",
};

extern "C" bool
presence_valid_state(const string_view &state)
{
	return std::any_of(begin(valid_states), end(valid_states), [&state]
	(const string_view &valid)
	{
		return state == valid;
	});
}

static void handle_edu_m_presence_(const m::event &, const m::edu::m_presence &edu);
static void handle_edu_m_presence(const m::event &);

const m::hook
_m_presence_eval
{
	handle_edu_m_presence,
	{
		{ "_site",   "vm.eval"     },
		{ "type",    "m.presence"  },
	}
};

void
handle_edu_m_presence(const m::event &event)
{
	const json::object &content
	{
		at<"content"_>(event)
	};

	const json::array &push
	{
		content.get("push")
	};

	for(const json::object &presence : push)
		handle_edu_m_presence_(event, presence);
}

void
handle_edu_m_presence_(const m::event &event,
                       const m::edu::m_presence &object)
{
	const m::user::id &user_id
	{
		at<"user_id"_>(object)
	};

	if(user_id.host() != at<"origin"_>(event))
	{
		log::warning
		{
			"Ignoring %s from %s for user %s",
			at<"type"_>(event),
			at<"origin"_>(event),
			string_view{user_id}
		};

		return;
	}

	const auto &presence
	{
		at<"presence"_>(object)
	};

	const auto evid
	{
		m::presence::set(object)
	};

	log::debug
	{
		"%s | %s %s [%s] %zd seconds ago => %s",
		at<"origin"_>(event),
		string_view{user_id},
		presence,
		json::get<"currently_active"_>(object)? "active"_sv : "inactive"_sv,
		json::get<"last_active_ago"_>(object) / 1000L,
		string_view{evid}
	};
}
