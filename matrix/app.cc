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
	static const auto not_executable{[]
	(const std::string &file)
	{
		return !fs::is_exec(file);
	}};

	std::vector<std::string> files
	{
		bool(path)?
			fs::ls(string_view(path)):
			std::vector<std::string>{}
	};

	bin = std::set<std::string>
	{
		begin(files), std::remove_if(begin(files), end(files), not_executable)
	};
}

void
ircd::m::app::fini()
{
	std::vector<app *> apps
	{
		std::begin(list), std::end(list)
	};

	for(auto *const &app : apps)
		delete app;
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
,argv
{
	std::begin(arg), std::end(arg)
}
,child
{
	argv
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
	const auto event_id
	{
		m::event_id(event_idx)
	};

	const auto room_id
	{
		m::room_id(event_idx)
	};

	const auto user_id
	{
		m::get(event_idx, "sender")
	};

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

	char buf alignas(4096) [16_KiB];
	for(run::barrier<ctx::interrupted>{};; )
	{
		const string_view &output
		{
			read(child, buf)
		};

		if(empty(output))
		{
			child.join();
			return;
		}

		const auto message_id
		{
			m::notice(room_id, user_id, output)
		};

		log::debug
		{
			log, "app:%lu output %zu bytes in %s to %s :%s%s",
			event_idx,
			size(output),
			string_view{message_id},
			string_view{room_id},
			trunc(output, 64),
			size(output) > 64? "..."_sv: ""_sv,
		};
	}
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
