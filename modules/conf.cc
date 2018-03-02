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
	"Configuration module"
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
_conf_set(const m::event &event,
          const string_view &key,
          const string_view &val)
try
{
	conf::set(key, val);

	const auto event_id
	{
		send(conf_room, at<"sender"_>(event), key,
		{
			{ "value", val }
		})
	};

	char kvbuf[768];
	notice(m::control, fmt::sprintf
	{
		kvbuf, "[%s] %s = %s", string_view{event_id}, key, val
	});
}
catch(const std::exception &e)
{
	notice(m::control, e.what());
}

static void
_conf_get(const m::event &event,
          const string_view &key)
try
{
	char vbuf[256];
	const auto value
	{
		conf::get(key, vbuf)
	};

	char kvbuf[512];
	notice(m::control, fmt::sprintf
	{
		kvbuf, "%s = %s", key, value
	});
}
catch(const std::exception &e)
{
	notice(m::control, e.what());
}

static void
_conf_list(const m::event &event)
try
{
	char val[512];
	std::stringstream ss;
	for(const auto &p : conf::items)
		ss << std::setw(32) << std::right << p.first
		   << " = " << p.second->get(val)
		   << "\n";

	notice(m::control, ss.str());
}
catch(const std::exception &e)
{
	notice(m::control, e.what());
}

static void
_update_conf(const m::event &event)
noexcept
{
	const auto &content
	{
		at<"content"_>(event)
	};

	if(unquote(content.get("msgtype")) == "m.notice")
		return;

	const string_view &body
	{
		unquote(content.at("body"))
	};

	string_view tokens[4];
	const size_t count
	{
		ircd::tokens(body, ' ', tokens)
	};

	const auto &cmd{tokens[0]};
	const auto &key{tokens[1]};
	const auto &val{tokens[3]};

	if(cmd == "set" && count >= 4)
		return _conf_set(event, key, val);

	if(cmd == "get" && count >= 2)
		return _conf_get(event, key);

	if(cmd == "conf")
		return _conf_list(event);
}

const m::hook
_update_conf_hook
{
	{
		{ "_site",       "vm notify"           },
		{ "room_id",     "!control:zemos.net"  },
		{ "type",        "m.room.message"      },
	},

	_update_conf
};

static void
_create_conf_room(const m::event &)
{
	m::create(conf_room_id, m::me.user_id);
}

const m::hook
_create_conf_hook
{
	{
		{ "_site",       "vm notify"       },
		{ "room_id",     "!ircd:zemos.net" },
		{ "type",        "m.room.create"   },
	},

	_create_conf_room
};
