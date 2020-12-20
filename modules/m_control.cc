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

static void
_cmd__die(const m::event &event,
          const string_view &line)
{
	static ios::descriptor descriptor
	{
		"ircd.m.control.die"
	};

	ircd::dispatch
	{
		descriptor, ios::defer, ircd::quit
	};

	ctx::yield();
}

static void
command_control(const m::event &event,
                m::vm::eval &)
noexcept try
{
	if(json::get<"room_id"_>(event) != control_room_id) //TODO: XXX see hook
		return;

	const auto &content
	{
		at<"content"_>(event)
	};

	const json::string &body
	{
		content.at("body")
	};

	const string_view &cmd
	{
		token(body, ' ', 0, {})
	};

	switch(hash(cmd))
	{
		case "die"_:
			return _cmd__die(event, tokens_after(body, ' ', 0));
	}

	const ircd::module console_module
	{
		"console"
	};

	using prototype = int (std::ostream &, const string_view &, const string_view &);
	mods::import<prototype> command
	{
		console_module, "console_command"s
	};

	std::ostringstream out;
	out.exceptions(out.badbit | out.failbit | out.eofbit);

	out << "<pre>";
	static const string_view opts{"html"}; try
	{
		command(out, body, opts);
	}
	catch(const std::exception &e)
	{
		const ctx::exception_handler eh;
		notice(control_room, e.what());
		return;
	}

	out << "</pre>";

	std::string str
	{
		out.str().substr(0, 32_KiB)
	};

	const string_view alt //TODO: X
	{
		"no alt text"
	};

	msghtml(control_room, m::me(), str, alt, "m.notice");
}
catch(const std::exception &e)
{
	const ctx::exception_handler eh;
	notice(control_room, e.what());
}

m::hookfn<m::vm::eval &>
command_control_hook
{
	command_control,
	{
		{ "_site",       "vm.effect"       },
//		{ "room_id",     "!control"        }, //TODO: XXX
		{ "type",        "m.room.message"  },
		{ "content",
		{
			{ "msgtype", "m.text" }
		}}
	}
};

static void
create_control_room(const m::event &,
                    m::vm::eval &)
{
	create(control_room_id, m::me());
	join(control_room, m::me());
	send(control_room, m::me(), "m.room.name", "",
	{
		{ "name", "Control Room" }
	});

	notice(control_room, m::me(), "Welcome to the control room.");
	notice(control_room, m::me(), "I am the daemon. You can talk to me in this room by highlighting me.");
}

m::hookfn<m::vm::eval &>
create_control_hook
{
	create_control_room,
	{
		{ "_site",       "vm.effect"      },
		{ "room_id",     "!ircd"          },
		{ "type",        "m.room.create"  },
	}
};
