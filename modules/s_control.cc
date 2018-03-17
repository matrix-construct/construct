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
	"Server Control"
};

const ircd::m::room::id::buf
control_room_id
{
	"!control", ircd::my_host()
};

const m::room
control_room
{
	control_room_id
};

const ircd::m::room::id::buf
conf_room_id
{
	"!conf", ircd::my_host()
};

static void
_conf_set(const m::event &event,
          const string_view &key,
          const string_view &val)
try
{
	const auto event_id
	{
		send(conf_room_id, at<"sender"_>(event), "ircd.conf.item", key,
		{
			{ "value", val }
		})
	};

	char kvbuf[768];
	notice(control_room, fmt::sprintf
	{
		kvbuf, "[%s] %s = %s", string_view{event_id}, key, val
	});
}
catch(const std::exception &e)
{
	notice(control_room, e.what());
}

static void
_conf_get(const m::event &event,
          const string_view &key)
try
{
	const m::room conf_room{conf_room_id};
	conf_room.get("ircd.conf.item", key, [&key]
	(const m::event &event)
	{
		const auto &value
		{
			unquote(at<"content"_>(event).at("value"))
		};

		char kvbuf[256];
		notice(control_room, fmt::sprintf
		{
			kvbuf, "%s = %s", key, value
		});
	});
}
catch(const std::exception &e)
{
	notice(control_room, e.what());
}

static void
_conf_list(const m::event &event)
try
{
	char val[512];
	std::stringstream ss;
	ss << "<pre>";
	for(const auto &p : conf::items)
		ss << std::setw(32) << std::right << p.first
		   << " = " << p.second->get(val)
		   << "<br />";
	ss << "</pre>";

	msghtml(control_room, m::me.user_id, ss.str());
}
catch(const std::exception &e)
{
	notice(control_room, e.what());
}

static void
_cmd__conf(const m::event &event,
           const string_view &line)
{
	string_view tokens[4];
	const size_t count
	{
		ircd::tokens(line, ' ', tokens)
	};

	const auto &cmd{tokens[0]};
	const auto &key{tokens[1]};
	const auto &val{tokens[3]};

	if(cmd == "set" && count >= 4)
		return _conf_set(event, key, val);

	if(cmd == "get" && count >= 2)
		return _conf_get(event, key);

	if(cmd == "list")
		return _conf_list(event);
}

static void
_cmd__die(const m::event &event,
          const string_view &line)
{
	ircd::post([]
	{
		ircd::quit();
	});

	ctx::yield();
}

static void
command_control(const m::event &event)
noexcept
{
	const auto &content
	{
		at<"content"_>(event)
	};

	const string_view &body
	{
		unquote(content.at("body"))
	};

	const string_view &cmd
	{
		token(body, ' ', 0, {})
	};

	switch(hash(cmd))
	{
		case hash("conf"):
			return _cmd__conf(event, tokens_after(body, ' ', 0));

		case hash("die"):
			return _cmd__die(event, tokens_after(body, ' ', 0));
	}
}

const m::hook
command_control_hook
{
	command_control,
	{
		{ "_site",       "vm.notify"       },
		{ "room_id",     "!control"        },
		{ "type",        "m.room.message"  },
		{ "content",
		{
			{ "msgtype", "m.text" }
		}}
	}
};

static void
create_control_room(const m::event &)
{
	create(control_room_id, m::me.user_id);
	join(control_room, m::me.user_id);
	send(control_room, m::me.user_id, "m.room.name", "",
	{
		{ "name", "Control Room" }
	});

	notice(control_room, m::me.user_id, "Welcome to the control room.");
	notice(control_room, m::me.user_id, "I am the daemon. You can talk to me in this room by highlighting me.");
}

const m::hook
create_control_hook
{
	create_control_room,
	{
		{ "_site",       "vm.notify"      },
		{ "room_id",     "!ircd"          },
		{ "type",        "m.room.create"  },
	}
};
