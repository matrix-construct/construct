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
	"Server Configuration"
};

const ircd::m::room::id::buf
conf_room_id
{
	"conf", ircd::my_host()
};

m::room
conf_room
{
	conf_room_id
};

static void
update_conf(const m::event &event)
noexcept
{
	const auto &content
	{
		at<"content"_>(event)
	};

	const auto &key
	{
		at<"state_key"_>(event)
	};

	const string_view &value
	{
		unquote(content.at("value"))
	};

	log::debug
	{
		"Updating conf [%s] => %s", key, value
	};

	ircd::conf::set(key, value);
}

const m::hook
update_conf_hook
{
	update_conf,
	{
		{ "_site",       "vm.notify"       },
		{ "room_id",     "!conf"           },
		{ "type",        "ircd.conf.item"  },
	}
};

static void
init_conf_items(const m::event &)
{
	const m::room::state state
	{
		conf_room
	};

	state.for_each("ircd.conf.item", []
	(const m::event &event)
	{
		update_conf(event);
	});
}

const m::hook
init_conf_items_hook
{
	init_conf_items,
	{
		{ "_site",       "vm.notify"      },
		{ "room_id",     "!ircd"          },
		{ "type",        "m.room.member"  },
		{ "membership",  "join"           },
		{ "state_key",   "@ircd"          },
	}
};

static void
create_conf_item(const string_view &key,
                 const conf::item<> &item)
{
	thread_local char vbuf[4_KiB];
	const string_view &val
	{
		item.get(vbuf)
	};

	send(conf_room, m::me.user_id, "ircd.conf.item", key,
	{
		{ "value", val }
	});
}

static void
create_conf_room(const m::event &)
{
	m::create(conf_room_id, m::me.user_id);

	for(const auto &p : conf::items)
	{
		const auto &key{p.first};
		const auto &item{p.second}; assert(item);
		create_conf_item(key, *item);
	}
}

const m::hook
create_conf_room_hook
{
	create_conf_room,
	{
		{ "_site",       "vm.notify"      },
		{ "room_id",     "!ircd"          },
		{ "type",        "m.room.create"  },
	}
};
