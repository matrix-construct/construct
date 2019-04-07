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

mapi::header
IRCD_MODULE
{
	"Server Command"
};

static std::pair<string_view, string_view>
execute_command(const mutable_buffer &buf,
                const m::user &user,
                const m::room &room,
                const string_view &cmd);

static void
handle_command(const m::event &event,
               m::vm::eval &eval);

const m::hookfn<m::vm::eval &>
command_hook
{
	handle_command,
	{
		{ "_site",    "vm.effect"       },
		{ "type",     "ircd.cmd"        },
		{ "origin",   my_host()         },
	}
};

void
handle_command(const m::event &event,
               m::vm::eval &eval)
try
{
	const m::user user
	{
		at<"sender"_>(event)
	};

	if(!m::my(user.user_id))
		return;

	const json::object &content
	{
		json::get<"content"_>(event)
	};

	const m::user::room user_room
	{
		user
	};

	if(json::get<"room_id"_>(event) != user_room.room_id)
		return;

	const m::room::id &room_id
	{
		unquote(content.at("room_id"))
	};

	const json::string &input
	{
		content.at("body")
	};

	if(!startswith(input, "\\\\"))
		return;

	const string_view &cmd
	{
		lstrip(input, "\\\\")
	};

	log::debug
	{
		m::log, "Server command from %s in %s :%s",
		string_view{room_id},
		string_view{user.user_id},
		cmd
	};

	const unique_buffer<mutable_buffer> buf
	{
		32_KiB
	};

	const auto &[html, alt]
	{
		execute_command(buf, user, room_id, cmd)
	};

	m::send(user_room, m::me, "ircd.cmd.result",
	{
		{ "msgtype",         "m.text"                  },
		{ "format",          "org.matrix.custom.html"  },
		{ "body",            { alt,  json::STRING }    },
		{ "formatted_body",  { html, json::STRING }    },
		{ "room_id",         room_id                   },
		{ "input",           input                     },
    });
}
catch(const std::exception &e)
{
	throw;
}

std::pair<string_view, string_view>
execute_command(const mutable_buffer &buf,
                const m::user &user,
                const m::room &room,
                const string_view &cmd)
try
{
	const string_view out{fmt::sprintf
	{
		buf, "unknown command :%s", cmd
	}};

	const string_view alt
	{
		"no alt text"
	};

	return { out, alt };
}
catch(const std::exception &e)
{
	log::error
	{
		m::log, "Server command from %s in %s '%s' :%s",
		string_view{room.room_id},
		string_view{user.user_id},
		cmd,
		e.what()
	};

	const size_t len
	{
		copy(buf, string_view(e.what()))
	};

	return
	{
		{ data(buf), len },
		{ data(buf), len }
	};
}
