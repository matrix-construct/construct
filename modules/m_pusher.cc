// The Construct
//
// Copyright (C) The Construct Developers, Authors & Contributors
// Copyright (C) 2016-2020 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

namespace ircd::m::push
{
	static void handle_event(const m::event &, vm::eval &);
	static void worker_handle();
	static void worker();

	static std::deque<ctx::future<http::code>> completion;
	extern hookfn<vm::eval &> hook_event;
	extern ctx::dock worker_dock;
	extern context worker_context;
}

ircd::mapi::header
IRCD_MODULE
{
	"Matrix 13.13 :Push Notifications Pusher",
};

decltype(ircd::m::push::worker_context)
ircd::m::push::worker_context
{
	"m.pusher", 256_KiB, worker,
};

decltype(ircd::m::push::hook_event)
ircd::m::push::hook_event
{
	handle_event,
	{
		{ "_site", "vm.effect" },
	}
};

void
ircd::m::push::worker()
try
{
    run::barrier<ctx::interrupted>{}; do
    {
		worker_dock.wait([]
		{
			return !completion.empty();
		});

    }
    while(run::level == run::level::RUN);
}
catch(const std::exception &e)
{
	log::critical
	{
		log, "Worker unhandled :%s",
		e.what(),
	};
}

void
ircd::m::push::worker_handle()
try
{

}
catch(const std::exception &e)
{
	log::error
	{
		log, "Pusher worker :%s",
		e.what(),
	};
}

void
ircd::m::push::handle_event(const m::event &event,
                            vm::eval &eval)
try
{
	// All pusher notifications are generated from internal rooms only
	if(!eval.room_internal)
		return;

	const auto &type
	{
		json::get<"type"_>(event)
	};

	// Filter out all push notification types by prefix
	if(!startswith(type, "ircd.push.note"))
		return;

	const json::string user_id
	{
		json::get<"content"_>(event).get("user_id")
	};

	if(unlikely(!user_id))
		return;

	const m::user user
	{
		user_id
	};

	// The event has to be in the user's room and not some other room
	if(unlikely(!m::user::room::is(at<"room_id"_>(event), user)))
		return;

	// Tweak type; room apropos
	const auto &[only, room_id]
	{
		split(lstrip(type, "ircd.push.note."), '!')
	};

	const m::user::pushers pushers
	{
		user
	};

	pushers.for_each([&]
	(const event::idx &pusher_idx, const string_view &pushkey, const push::pusher &pusher)
	{
		const json::object &data
		{
			json::get<"data"_>(pusher)
		};

		const json::string &url
		{
			data["url"]
		};

		if(!rfc3986::valid(std::nothrow, rfc3986::parser::uri, url))
		{
			log::derror
			{
				log, "Pusher in idx:%lu data.url not a valid uri '%s'",
				pusher_idx,
				url,
			};

			return true;
		}

		log::debug
		{
			log, "Notifying `%s' ",
			url,
		};

		return true;
	});
}
catch(const ctx::interrupted &)
{
	throw;
}
catch(const std::exception &e)
{
	log::error
	{
		log, "Pusher pushing by %s :%s",
		string_view{event.event_id},
		e.what(),
	};
}
