// Matrix Construct
//
// Copyright (C) Matrix Construct Developers, Authors & Contributors
// Copyright (C) 2016-2019 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

#include "app.h"

ircd::mapi::header
IRCD_MODULE
{
	"Application Services",
	ircd::m::app::init,
	ircd::m::app::fini
};

decltype(ircd::m::app::ns::users)
ircd::m::app::ns::users;

decltype(ircd::m::app::ns::aliases)
ircd::m::app::ns::aliases;

decltype(ircd::m::app::ns::rooms)
ircd::m::app::ns::rooms;

decltype(ircd::m::app::app_room_id)
ircd::m::app::app_room_id
{
	"app", my_host()
};

void
ircd::m::app::fini()
{

}

void
ircd::m::app::init()
{
	if(!m::exists(app_room_id))
		m::create(app_room_id, m::me, "internal");

	init_apps();
}

void
ircd::m::app::init_apps()
{
	const m::room::state room
	{
		app_room_id
	};

	room.for_each("ircd.app", []
	(const string_view &type, const string_view &id, const event::idx &event_idx)
	{
		m::app::config::get(std::nothrow, id, [&id]
		(const json::object &config)
		{
			init_app(id, config);
		});

		return true;
	});
}

void
ircd::m::app::init_app(const string_view &id,
                       const json::object &config)
try
{

}
catch(const std::exception &e)
{
	log::error
	{
		m::log, "Failed to init appservice '%s' :%s", id, e.what()
	};
}

std::string
IRCD_MODULE_EXPORT
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
IRCD_MODULE_EXPORT
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
IRCD_MODULE_EXPORT
ircd::m::app::config::get(const string_view &id,
                          const event::fetch::view_closure &closure)
{
	if(!get(std::nothrow, id, closure))
		throw m::NOT_FOUND
		{
			"Configuration for appservice '%s' not found.", id
		};
}

bool
IRCD_MODULE_EXPORT
ircd::m::app::config::get(std::nothrow_t,
                          const string_view &id,
                          const event::fetch::view_closure &closure)
{
	return m::get(std::nothrow, idx(std::nothrow, id), "content", [&closure]
	(const json::object &content)
	{
		closure(content);
	});
}

ircd::m::event::idx
IRCD_MODULE_EXPORT
ircd::m::app::config::idx(const string_view &id)
{
	const m::room::state state{app_room_id};
	return state.get(std::nothrow, "ircd.app", id);
}

ircd::m::event::idx
IRCD_MODULE_EXPORT
ircd::m::app::config::idx(std::nothrow_t,
                          const string_view &id)
{
	const m::room::state state{app_room_id};
	return state.get("ircd.app", id);
}

bool
IRCD_MODULE_EXPORT
ircd::m::app::exists(const string_view &id)
{
	const m::room::state state{app_room_id};
	return state.has("ircd.app", id);
}
