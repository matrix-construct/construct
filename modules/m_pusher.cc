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
	static long count_missed_calls(const user &, const room &, const event::idx &);
	static long count_missed_calls(const user &, const event::idx &);
	static long count_unread(const user &, const room &, const event::idx &);
	static long count_unread(const user &, const event::idx &);

	static void make_content_devices(json::stack::array &, const pusher &, const event::idx &);
	static void make_content_counts(json::stack::object &, const user &, const event::idx &);
	static void make_content(json::stack::object &, const user &, const room &, const event &, const event::idx &, const pusher &, const event::idx &);

	static bool notify_email(const user &, const room &, const event::fetch &, const pusher &, const event::idx &pusher_idx);
	static bool notify_http(const user &, const room &, const event::fetch &, const pusher &, const event::idx &pusher_idx);
	static bool notify(const user &, const room &, const event::fetch &, const pusher &, const event::idx &pusher_idx);
	static void handle_event(const event &, vm::eval &);

	static bool complete(request &);
	static void worker();
	static void fini();

	static server::request request_skip;
	extern hookfn<vm::eval &> hook_event;
	extern context worker_context;
}

ircd::mapi::header
IRCD_MODULE
{
	"Matrix 13.13 :Push Notifications Pusher",
	nullptr,
	ircd::m::push::fini,
};

decltype(ircd::m::push::worker_context)
ircd::m::push::worker_context
{
	"m.pusher",
	256_KiB,
	context::WAIT_JOIN,
	worker,
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
ircd::m::push::fini()
{
	for(auto *const &req : request::list)
		server::cancel(req->req);

	worker_context.terminate();
	request::dock.notify_all();
}

void
ircd::m::push::worker()
try
{
	// Wait for run::level RUN before entering work loop.
    run::barrier<ctx::interrupted>{};
    const ctx::uninterruptible ui; do
    {
		static const auto valid{[](const auto *const &req)
		{
			return !!req->req;
		}};

		// Wait for at least one active request in the list
		request::dock.wait([]
		{
			if(std::any_of(begin(request::list), end(request::list), valid))
				return true;

			if(ctx::termination(worker_context))
				return true;

			return false;
		});

		if(request::list.empty() && ctx::termination(worker_context))
			break;

		auto next
		{
			ctx::when_any(begin(request::list), end(request::list), []
			(auto &it) -> server::request &
			{
				return !(*it)->req? request_skip: (*it)->req;
			})
		};

		// Wait for the next activity
		if(!next.wait(milliseconds(250), std::nothrow))
			continue;

		const std::lock_guard lock
		{
			request::mutex
		};

		const auto it
		{
			next.get()
		};

		assert(it != end(request::list));
		if(unlikely(it == end(request::list)))
			continue;

		std::unique_ptr<request> req
		{
			*it
		};

		// Handle completion
		if(!complete(*req))
			req.release();
	}
	while(1);
}
catch(const ctx::interrupted &)
{
	throw;
}
catch(const std::exception &e)
{
	log::critical
	{
		log, "Worker unhandled :%s",
		e.what(),
	};
}

bool
ircd::m::push::complete(request &req)
try
{
	req.code = req.req.get();
	req.response = json::object
	{
		req.req.in.content
	};

	const auto level
	{
		http::category(req.code) == http::category::SUCCESS?
			log::level::DEBUG:
			log::level::DERROR
	};

	log::logf
	{
		log, level,
		"Request id:%lu [%u] notified %s `%s'",
		req.id,
		uint(req.code),
		req.url.remote,
		req.url.path,
	};

	return true;
}
catch(const std::exception &e)
{
	log::error
	{
		log, "Request id:%lu [---] notifying %s `%s' :%s",
		req.id,
		req.url.remote,
		req.url.path,
		e.what(),
	};

	return true;
}

void
ircd::m::push::handle_event(const m::event &event,
                            vm::eval &eval)
try
{
	// Pushing disabled by configuration
	if(!request::enable)
		return;

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

	const json::string &user_id
	{
		json::get<"content"_>(event).get("user_id")
	};

	if(unlikely(!user_id))
		return;

	const m::user user
	{
		user_id
	};

	const m::room::id user_room_id
	{
		at<"room_id"_>(event)
	};

	// The event has to be in the user's room and not some other room
	if(unlikely(!m::user::room::is(user_room_id, user)))
		return;

	const auto subject
	{
		m::user::notifications::unmake_type(type)
	};

	const m::room room
	{
		subject.room_id
	};

	const event::idx event_idx
	{
		json::get<"content"_>(event).at<event::idx>("event_idx")
	};

	// Note the subject event data is not fetched yet. If the user
	// has any pushers it's fetched on the first and reused subsequently.
	m::event::fetch push_event;
	const m::user::pushers pushers
	{
		user
	};

	pushers.for_each([&user, &room, &event_idx, &push_event]
	(const event::idx &pusher_idx, const string_view &pushkey, const push::pusher &pusher)
	{
		// If the subject event data hasn't been fetched yet, do it here.
		if(!push_event.valid)
			m::seek(push_event, event_idx);

		assert(push_event.valid);
		return notify(user, room, push_event, pusher, pusher_idx);
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
		log, "Pushing %s :%s",
		string_view{event.event_id},
		e.what(),
	};
}

bool
ircd::m::push::notify(const m::user &user,
                      const m::room &room,
                      const m::event::fetch &event,
                      const pusher &pusher,
                      const m::event::idx &pusher_idx)
try
{
	const json::string &kind
	{
		json::get<"kind"_>(pusher)
	};

	if(kind == "http")
		return notify_http(user, room, event, pusher, pusher_idx);

	if(kind == "email")
		return notify_email(user, room, event, pusher, pusher_idx);

	return true;
}
catch(const ctx::interrupted &)
{
	throw;
}
catch(const std::exception &e)
{
	log::error
	{
		log, "Notify to pusher:%lu by %s in %s for %s :%s",
		pusher_idx,
		string_view{user.user_id},
		string_view{room.room_id},
		string_view{event.event_id},
		e.what(),
	};

	return true;
}

bool
ircd::m::push::notify_http(const m::user &user,
                           const m::room &room,
                           const m::event::fetch &event,
                           const pusher &pusher,
                           const m::event::idx &pusher_idx)
{
	const std::lock_guard lock
	{
		request::mutex
	};

	auto req
	{
		std::make_unique<request>()
	};

	window_buffer wb{req->buf};
	mutable_buffer buf{req->buf};
	req->event_idx = event.event_idx;

	// Target URL copied to request and verified on assignment
	wb([&req, &pusher](const mutable_buffer &buf)
	{
		const json::object data
		{
			json::get<"data"_>(pusher)
		};

		const string_view url
		{
			buffer::data(buf), copy(buf, json::string(data["url"]))
		};

		req->url = url;
		return url;
	});
	consume(buf, wb.consumed());
	wb = window_buffer(wb.remains());

	// Compose request content
	wb([&](const mutable_buffer &buf)
	{
		json::stack out{buf};
		{
			json::stack::object top{out};
			make_content(top, user, room, event, event.event_idx, pusher, pusher_idx);
		}

		req->content = out.completed();
		return string_view{req->content};
	});
	consume(buf, wb.consumed());
	wb = window_buffer(wb.remains());

	const net::hostport target
	{
		req->url
    };

	// Compose request head
	http::request
	{
		wb,
		host(target),
		"POST",
		req->url.path,
		size(string_view(req->content)),
		"application/json; charset=utf-8"_sv,
	};

	// Outputs
	server::out out;
	out.head = wb.completed();
	consume(buf, wb.consumed());
	out.content = req->content;

	// Inputs to remaining buffer
	server::in in;
	in.head = buf;
	in.content = in.head;

	// Start request
	static server::request::opts sopts;
	sopts.http_exceptions = false;

	req->req = server::request
	{
		target, std::move(out), std::move(in), &sopts
	};

	log::debug
	{
		log, "Request id:%lu to pusher[%s...] by %s in %s for %s",
		req->id,
		trunc(json::get<"pushkey"_>(pusher), 16),
		string_view{user.user_id},
		string_view{room.room_id},
		string_view{event.event_id},
	};

	request::dock.notify();
	req.release();
	return true;
}

bool
ircd::m::push::notify_email(const m::user &user,
                            const m::room &room,
                            const m::event::fetch &event,
                            const pusher &pusher,
                            const m::event::idx &pusher_idx)
{
	return true;
}

void
ircd::m::push::make_content(json::stack::object &top,
                            const m::user &user,
                            const m::room &room,
                            const m::event &event,
                            const event::idx &event_idx,
                            const pusher &pusher,
                            const m::event::idx &pusher_idx)
{
	const m::user sender
	{
		json::get<"sender"_>(event)
	};

	const bool event_id_only
	{
		json::string(json::get<"data"_>(pusher).get("format")) == "event_id_only"
	};

	json::stack::object note
	{
		top, "notification"
	};

	json::stack::member
	{
		note, "event_id", event.event_id
	};

	json::stack::member
	{
		note, "room_id", room.room_id
	};

	json::stack::member
	{
		note, "sender", json::get<"sender"_>(event)
	};

	json::stack::member
	{
		note, "type", json::get<"type"_>(event)
	};

	if(!event_id_only && (false)) // buffer size?
		json::stack::member
		{
			note, "content", json::get<"content"_>(event)
		};

	json::stack::member
	{
		note, "prio", string_view
		{
			"high" //TODO: xxx
		}
	};

	// Devices
	{
		json::stack::array devices
		{
			note, "devices"
		};

		make_content_devices(devices, pusher, pusher_idx);
	}

	// Counts
	{
		json::stack::object counts
		{
			note, "counts"
		};

		make_content_counts(counts, user, event_idx);
	}

	char room_name_buf[256];
	const auto room_name
	{
		m::display_name(room_name_buf, room)
	};

	if(room_name)
		json::stack::member
		{
			note, "room_name", room_name
		};

	char sender_name_buf[256];
	const auto sender_display_name
	{
		user::profile(sender).get(sender_name_buf, "displayname")
	};

	if(sender_display_name)
		json::stack::member
		{
			note, "sender_display_name", sender_display_name
		};

	json::stack::member
	{
		note, "user_is_target", json::value{bool
		{
			json::get<"type"_>(event) == "m.room.member" &&
			json::get<"state_key"_>(event) == user.user_id
		}}
	};
}

void
ircd::m::push::make_content_devices(json::stack::array &devices,
                                    const pusher &pusher,
                                    const m::event::idx &pusher_idx)
{
	json::stack::object device
	{
		devices
	};

	json::stack::member
	{
		device, "app_id", at<"app_id"_>(pusher)
	};

	json::stack::member
	{
		device, "pushkey", at<"pushkey"_>(pusher)
	};

	json::stack::member
	{
		device, "data", json::get<"data"_>(pusher)
	};

	time_t pushkey_ts;
	if(m::get(pusher_idx, "origin_server_ts", pushkey_ts))
		json::stack::member
		{
			device, "pushkey_ts", json::value
			{
				pushkey_ts
			}
		};

	return; //TODO: XXX get tweaks from matched push rule
	json::stack::member
	{
		device, "tweaks", json::empty_object
	};
}

void
ircd::m::push::make_content_counts(json::stack::object &counts,
                                   const user &user,
                                   const event::idx &event_idx)
{
	const auto unread
	{
		count_unread(user, event_idx)
	};

	json::stack::member
	{
		counts, "unread", json::value
		{
			unread
		}
	};

	long missed_calls
	{
		count_missed_calls(user, event_idx)
	};

	if(missed_calls)
		json::stack::member
		{
			counts, "missed_calls", json::value
			{
				missed_calls
			}
		};
}

long
ircd::m::push::count_unread(const user &user,
                            const event::idx &event_idx)
{
	const m::user::rooms rooms
	{
		user
	};

	long ret(0);
	rooms.for_each("join", [&ret, &user, &event_idx]
	(const m::room &room, const auto &membership)
	{
		ret += count_unread(user, room, event_idx);
	});

	return ret;
}

long
ircd::m::push::count_unread(const user &user,
                            const room &room,
                            const event::idx &event_idx)
{
	event::id::buf read_buf;
	const auto read_idx
	{
		index(std::nothrow, receipt::get(read_buf, room, user))
	};

	const event::idx_range unread_range
	{
		std::minmax(read_idx, event_idx)
	};

	const user::notifications notifications
	{
		user
	};

	user::notifications::opts opts;
	opts.room_id = room.room_id;
	opts.from = unread_range.second;
	opts.to = unread_range.first;
	const auto unread
	{
		notifications.count(opts)
	};

	return unread;
}

long
ircd::m::push::count_missed_calls(const user &user,
                                  const event::idx &event_idx)
{
	return 0L; //TODO: XXX
}

long
ircd::m::push::count_missed_calls(const user &user,
                                  const room &room,
                                  const event::idx &event_idx)
{
	return 0L; //TODO: XXX
}
