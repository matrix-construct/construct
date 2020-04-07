// The Construct
//
// Copyright (C) The Construct Developers, Authors & Contributors
// Copyright (C) 2016-2020 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

decltype(ircd::m::app::log)
ircd::m::app::log
{
	"m.app"
};

bool
ircd::m::app::exists(const string_view &id)
{
	return config::idx(std::nothrow, id);
}

//
// config
//

std::string
ircd::m::app::config::get(const string_view &id)
{
	std::string ret;
	get(id, [&ret]
	(const string_view &str)
	{
		ret = str;
	});

	return ret;
}

std::string
ircd::m::app::config::get(std::nothrow_t,
                          const string_view &id)
{
	std::string ret;
	get(std::nothrow, id, [&ret]
	(const string_view &str)
	{
		ret = str;
	});

	return ret;
}

void
ircd::m::app::config::get(const string_view &id,
                          const event::fetch::view_closure &closure)
{
	if(!get(std::nothrow, id, closure))
		throw m::NOT_FOUND
		{
			"Configuration for appservice '%s' not found.",
			id
		};
}

bool
ircd::m::app::config::get(std::nothrow_t,
                          const string_view &id,
                          const event::fetch::view_closure &closure)
{
	const auto event_idx
	{
		idx(std::nothrow, id)
	};

	return m::get(std::nothrow, event_idx, "content", [&closure]
	(const json::object &content)
	{
		closure(content);
	});
}

ircd::m::event::idx
ircd::m::app::config::idx(const string_view &id)
{
	const m::room::id::buf app_room_id
	{
		"app", my_host()
	};

	const m::room::state state
	{
		app_room_id
	};

	return state.get(std::nothrow, "ircd.app", id);
}

ircd::m::event::idx
ircd::m::app::config::idx(std::nothrow_t,
                          const string_view &id)
{
	const m::room::id::buf app_room_id
	{
		"app", my_host()
	};

	const m::room::state state
	{
		app_room_id
	};

	return state.get("ircd.app", id);
}
