// Matrix Construct
//
// Copyright (C) Matrix Construct Developers, Authors & Contributors
// Copyright (C) 2016-2019 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

#include <util/params.h>

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

	if(!html && !alt)
		return;

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

static std::pair<string_view, string_view>
command__dash(const mutable_buffer &buf,
              const m::user &user,
              const m::room &room,
              const string_view &cmd);

static std::pair<string_view, string_view>
command__read(const mutable_buffer &buf,
              const m::user &user,
              const m::room &room,
              const string_view &cmd);

static std::pair<string_view, string_view>
command__version(const mutable_buffer &buf,
                 const m::user &user,
                 const m::room &room,
                 const string_view &cmd);

std::pair<string_view, string_view>
execute_command(const mutable_buffer &buf,
                const m::user &user,
                const m::room &room,
                const string_view &cmd)
try
{
	switch(hash(token(cmd, ' ', 0)))
	{
		case "version"_:
			return command__version(buf, user, room, cmd);

		case "read"_:
			return command__read(buf, user, room, cmd);

		case "dash"_:
			return command__dash(buf, user, room, cmd);

		default:
			break;
	}

	const string_view out{fmt::sprintf
	{
		buf, "unknown command :%s", cmd
	}};

	const string_view alt
	{
		out
	};

	return { out, alt };
}
catch(const m::error &e)
{
	const json::object &error
	{
		e.content
	};

	log::error
	{
		m::log, "Server command from %s in %s '%s' :%s :%s :%s",
		string_view{room.room_id},
		string_view{user.user_id},
		cmd,
		e.what(),
		unquote(error.get("errcode")),
		unquote(error.get("error")),
	};

	std::ostringstream out;
	pubsetbuf(out, buf);

	const string_view fg[] {"#FCFCFC", "#FFFFFF"};
	const string_view bg[] {"#A01810", "#C81810"};
	const string_view sp {"&nbsp;"};
	out
	    << "<h5>"
	    << "<font color=\"" << fg[0] << "\" data-mx-bg-color=\"" << bg[0] << "\">"
	    << "<b>"
	    << sp << sp << e.what() << sp << sp
	    << "</b>"
	    << "</font> "
	    << "<font color=\"" << fg[1] << "\" data-mx-bg-color=\"" << bg[1] << "\">"
	    << "<b>"
	    << sp << sp << unquote(error.get("errcode")) << sp << sp
	    << "</b>"
	    << "</font> "
	    << "</h5>"
	    << "<pre>"
	    << unquote(error.get("error"))
	    << "</pre>"
	    ;

	return
	{
		view(out, buf), e.what()
	};
}
catch(const http::error &e)
{
	log::error
	{
		m::log, "Server command from %s in %s '%s' :%s :%s",
		string_view{room.room_id},
		string_view{user.user_id},
		cmd,
		e.what(),
		e.content
	};

	std::ostringstream out;
	pubsetbuf(out, buf);

	const string_view fg[] {"#FCFCFC"};
	const string_view bg[] {"#A01810"};
	const string_view sp {"&nbsp;"};
	out
	    << "<h5>"
	    << "<font color=\"" << fg[0] << "\" data-mx-bg-color=\"" << bg[0] << "\">"
	    << "<b>"
	    << sp << sp << e.what() << sp << sp
	    << "</b>"
	    << "</font> "
	    << "</h5>"
	    << "<pre>"
	    << e.content
	    << "</pre>"
	    ;

	return
	{
		view(out, buf), e.what()
	};
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

std::pair<string_view, string_view>
command__version(const mutable_buffer &buf,
                 const m::user &user,
                 const m::room &room,
                 const string_view &cmd)
{
	std::ostringstream out;
	pubsetbuf(out, buf);

	const string_view sp {"&nbsp;"};
	out
	    << "<h1>"
	    << info::name
	    << "</h1>"
	    << "<pre><code>"
	    << info::version
	    << "</code></pre>"
	    ;

	const string_view alt
	{
		info::version
	};

	return { view(out, buf), alt };
}

std::pair<string_view, string_view>
command__read(const mutable_buffer &buf,
              const m::user &user,
              const m::room &room,
              const string_view &cmd)
{
	const params param{tokens_after(cmd, ' ', 0), " ",
	{
		"event_id", "[time]"
	}};

	const m::event::id::buf event_id
	{
		param["event_id"]?
			m::event::id::buf{param["event_id"]}:
			m::head(room)
	};

	const time_t &ms
	{
		param.at("[time]", ircd::time<milliseconds>())
	};

	m::receipt::read(room, user, event_id, ms);
	return {};
}

std::pair<string_view, string_view>
command__dash(const mutable_buffer &buf,
              const m::user &user,
              const m::room &room,
              const string_view &cmd)
{
	std::ostringstream out;
	pubsetbuf(out, buf);

	const string_view fg[] {"#E8E8E8", "#FFFFFF"};
	const string_view bg[] {"#303030", "#008000"};
	const string_view sp {"&nbsp;"};
	out
	    << "<h5>"
	    << "<font color=\"" << fg[0] << "\" data-mx-bg-color=\"" << bg[0] << "\">"
	    << "<b>"
	    << sp << sp << " CONSTRUCT STATUS " << sp << sp
	    << "</b>"
	    << "</font> "
	    << "<font color=\"" << fg[1] << "\" data-mx-bg-color=\"" << bg[1] << "\">"
	    << "<b>"
	    << sp << sp << " OK " << sp << sp
	    << "</b>"
	    << "</font> "
	    << "</h5>"
	    << "<pre>"
	    << " "
	    << "</pre>"
	    ;

	const string_view alt
	{
		"no alt text"
	};

	return { view(out, buf), alt };
}
