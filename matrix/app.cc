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

decltype(ircd::m::app::path)
ircd::m::app::path
{
	{ "name",      "ircd.m.app.path"  },
	{ "default",   string_view{}      },
	{ "persist",   false              },
};

decltype(ircd::m::app::autorun)
ircd::m::app::autorun
{
	{ "name",      "ircd.m.app.autorun" },
	{ "default",   true                 },
};

decltype(ircd::m::app::enable)
ircd::m::app::enable
{
	{ "name",      "ircd.m.app.enable" },
	{ "default",   true                },
};

decltype(ircd::m::app::bin)
ircd::m::app::bin;

template<>
decltype(ircd::util::instance_list<ircd::m::app>::allocator)
ircd::util::instance_list<ircd::m::app>::allocator{};

template<>
decltype(ircd::util::instance_list<ircd::m::app>::list)
ircd::util::instance_list<ircd::m::app>::list
{
	allocator
};

//
// init
//

void
ircd::m::app::init()
{
	if(!enable || !path || ircd::read_only)
		return;

	std::vector<std::string> files
	{
		fs::ls(string_view(path))
	};

	bin = std::set<std::string>
	{
		begin(files), std::remove_if(begin(files), end(files), []
		(const std::string &file)
		{
			return !fs::is_exec(file);
		})
	};

	log::debug
	{
		log, "Found %zu executable in `%s'",
		bin.size(),
		string_view{path},
	};

	if(!autorun)
	{
		log::warning
		{
			log, "Autorun is disabled by the configuration. Apps may still be executed manually.",
		};

		return;
	}

	events::type::for_each_in("ircd.app.run.auto", []
	(const string_view &, const event::idx &run_event_idx)
	{
		const m::event::fetch run_event
		{
			std::nothrow, run_event_idx
		};

		if(!run_event.valid || !my(run_event))
			return true;

		const m::room room
		{
			at<"room_id"_>(run_event)
		};

		const m::event::idx app_event_idx
		{
			room.get(std::nothrow, "ircd.app", at<"state_key"_>(run_event))
		};

		if(!app_event_idx)
			return true;

		const m::event::fetch app_event
		{
			std::nothrow, app_event_idx
		};

		if(!app_event.valid || !my(app_event))
			return true;

		log::debug
		{
			log, "Attempting app:%lu run.auto:%lu",
			app_event_idx,
			run_event_idx,
		};

		auto app
		{
			std::make_unique<m::app>(app_event_idx)
		};

		const auto pid
		{
			app->child.run()
		};

		app.release();
		return true;
	});
}

void
ircd::m::app::fini()
{
	std::vector<app *> apps
	{
		std::begin(list), std::end(list)
	};

	for(auto *const &app : apps)
	{
		app->child.join(15);
		delete app;
	}
}

//
// app::app
//

ircd::m::app::app(const m::event::idx &event_idx)
:event_idx
{
	event_idx
}
,feature
{
	m::get(event_idx, "content")
}
,config
{
	feature
}
,arg
{
	config.at("arg")
}
,binpath{[&]
{
	if(!path)
		throw m::FORBIDDEN
		{
			"Configure the 'ircd.m.app.path' to permit."
		};

	if(!enable)
		throw m::FORBIDDEN
		{
			"Configure 'ircd.m.app.enable' to permit."
		};

	const json::string file
	{
		arg.at(0)
	};

	const string_view part[2]
	{
		path, file
	};

	const auto ret
	{
		fs::path_string(path, part)
	};

	if(!bin.count(ret))
		throw m::NOT_FOUND
		{
			"Executable '%s' not found in bin directory at `%s'",
			file,
			string_view{path},
		};

	return ret;
}()}
,argv{[&]
{
	std::vector<json::string> ret
	{
		std::begin(arg), std::end(arg)
	};

	ret.at(0) = binpath;
	return ret;
}()}
,outbuf
{
	32_KiB
}
,child
{
	argv
}
,user_id
{
	m::get(event_idx, "sender")
}
,room_id
{
	m::room_id(event_idx)
}
,event_id
{
	m::event_id(event_idx)
}
,room_hook
{
	std::bind(&app::handle_room_message, this, ph::_1, ph::_2),
	{
		{ "_site",    "vm.eval"        },
		{ "type",     "m.room.message" },
		{ "room_id",  this->room_id    },
	}
}
,worker_context
{
	"m.app",
	512_KiB,
	std::bind(&app::worker, this)
}
{
}

ircd::m::app::~app()
noexcept
{
	worker_context.interrupt();
}

void
ircd::m::app::worker()
try
{
	child.dock.wait([this]
	{
		return child.pid >= 0;
	});

	log::info
	{
		log, "app:%lu starting %s in %s for %s @ `%s' id:%lu pid:%ld",
		event_idx,
		string_view{event_id},
		string_view{room_id},
		string_view{user_id},
		argv.at(0),
		child.id,
		child.pid,
	};

	run::barrier<ctx::interrupted>{}; do
	{
		if(!handle_stdout())
			break;
	}
	while(1);
}
catch(const std::exception &e)
{
	log::error
	{
		log, "app:%lu (%p) worker fatal :%s",
		event_idx,
		this,
		e.what(),
	};

	const ctx::exception_handler eh;
	child.join();
	return;
}

bool
ircd::m::app::handle_stdout()
{
	mutable_buffer buf(this->outbuf);
	consume(buf, copy(buf, "<pre>"_sv));
	const string_view output
	{
		read(this->child, buf)
	};

	consume(buf, size(output));
	if(empty(output))
	{
		log::debug
		{
			log, "app:%lu :end of file",
			this->event_idx,
		};

		return false;
	}

	consume(buf, copy(buf, "</pre>"_sv));
	const string_view &content
	{
		data(this->outbuf), data(buf)
	};

	const fmt::bsprintf<96> alt
	{
		"app:%lu wrote %zu bytes to stdout.",
		this->event_idx,
		size(output),
	};

	const auto message_id
	{
		!ircd::write_avoid?
			m::msghtml(room_id, user_id, content, string_view{alt}, "m.notice"):
			m::event::id::buf{}
	};

	log::debug
	{
		log, "app:%lu output %zu bytes in %s to %s :%s%s",
		event_idx,
		size(content),
		string_view{message_id},
		string_view{room_id},
		trunc(content, 64),
		size(content) > 64? "...": "",
	};

	return true;
}

void
ircd::m::app::handle_room_message(const event &event,
                                  vm::eval &)
{
	assert(at<"room_id"_>(event) == room_id);
	assert(at<"type"_>(event) == "m.room.message");

	if(at<"sender"_>(event) == user_id)
		return;

	const m::room::message msg
	{
		json::get<"content"_>(event)
	};

	if(json::get<"msgtype"_>(msg) != "m.text")
		return;

	if(!startswith(json::get<"body"_>(msg), user_id))
		return;

	string_view text
	{
		json::get<"body"_>(msg)
	};

	text = lstrip(text, user_id);
	text = lstrip(text, ':');
	text = lstrip(text, ' ');
	handle_stdin(event, text);
}

void
ircd::m::app::handle_stdin(const event &event,
                           const string_view &text)
{
	const string_view wrote
	{
		write(this->child, text)
	};

	const string_view nl
	{
		write(this->child, "\n"_sv)
	};

	log::debug
	{
		log, "app:%lu input %zu of %zu bytes from %s in %s :%s%s",
		event_idx,
		size(wrote) + size(nl),
		size(text) + 1,
		string_view{at<"sender"_>(event)},
		string_view{room_id},
		trunc(text, 64),
		size(text) > 64? "...": "",
	};
}
