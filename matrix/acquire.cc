// The Construct
//
// Copyright (C) The Construct Developers, Authors & Contributors
// Copyright (C) 2016-2020 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

namespace ircd::m::acquire
{
	struct result;
	using list = std::list<result>;

	static void start(const opts &, list &);
	static bool handle(const opts &, ctx::future<m::fetch::result> &);
	static bool handle(const opts &, list &);
	static void fetch_head(const opts &, list &);
	static bool fetch_missing(const opts &, list &);
	static void submit(const opts &, list &);
};

struct ircd::m::acquire::result
{
	ctx::future<m::fetch::result> future;
	event::id::buf event_id;

	result(ctx::future<m::fetch::result> &&future, const event::id &event_id)
	:future{std::move(future)}
	,event_id{event_id}
	{}
};

decltype(ircd::m::acquire::log)
ircd::m::acquire::log
{
	"m.acquire"
};

//
// execute::execute
//

ircd::m::acquire::execute::execute(const opts &opts)
{
	list fetching;

	// Branch to acquire head
	if(opts.head)
		fetch_head(opts, fetching);

	// Branch to acquire missing
	if(opts.missing)
		for(size_t i(0); i < opts.rounds; ++i)
			if(!fetch_missing(opts, fetching))
				break;

	// Complete all work before returning, otherwise everything
	// will be cancelled on unwind.
	while(handle(opts, fetching));
}

bool
ircd::m::acquire::fetch_missing(const opts &opts,
                                list &fetching)
{
	const auto top
	{
		m::top(opts.room.room_id)
	};

	m::room::events::missing missing
	{
		opts.room
	};

	bool ret(false);
	missing.for_each(opts.depth, [&opts, &fetching, &top, &ret]
	(const event::id &event_id, const int64_t &ref_depth, const event::idx &ref_idx)
	{
		// Bail if interrupted
		if(ctx::interruption_requested())
			return false;

		// Branch if we have to measure the viewportion
		if(opts.viewport_size)
		{
			const m::event::idx_range range
			{
				std::min(ref_idx, std::get<event::idx>(top)),
				std::max(ref_idx, std::get<event::idx>(top)),
			};

			// Bail if this event sits above the viewport.
			if(m::room::events::count(opts.room, range) > opts.viewport_size)
				return false;
		}

		auto _opts(opts);
		_opts.room.event_id = event_id;
		_opts.hint = opts.hint;
		submit(_opts, fetching);
		ret = true;
		return true;
	});

	return ret;
}

void
ircd::m::acquire::fetch_head(const opts &opts,
                             list &fetching)
{
	const auto handle_head{[&opts, &fetching]
	(const m::event &result)
	{
		// Bail if interrupted
		if(ctx::interruption_requested())
			return false;

		// Bail if the depth is below the window
		if(json::get<"depth"_>(result) < opts.depth.first)
			return false;

		auto _opts(opts);
		_opts.room.event_id = result.event_id;
		_opts.hint = json::get<"origin"_>(result);
		_opts.hint_only = true;
		submit(_opts, fetching);
		return true;
	}};

	const auto top
	{
		m::top(opts.room.room_id)
	};

	m::room::head::fetch::opts hfopts;
	hfopts.room_id = opts.room.room_id;
	hfopts.top = top;
	m::room::head::fetch
	{
		hfopts, handle_head
	};
}

void
ircd::m::acquire::submit(const opts &opts,
                         list &fetching)
{
	start(opts, fetching); do
	{
		if(!handle(opts, fetching))
			break;
	}
	while(fetching.size() > opts.fetch_width);
}

void
ircd::m::acquire::start(const opts &opts,
                        list &fetching)
{
	fetch::opts fopts;
	fopts.op = fetch::op::event;
	fopts.room_id = opts.room.room_id;
	fopts.event_id = opts.room.event_id;
	fopts.backfill_limit = 1;
	fopts.hint = opts.hint;
	fopts.attempt_limit = opts.hint_only;
	fetching.emplace_back(fetch::start(fopts), opts.room.event_id);
}

bool
ircd::m::acquire::handle(const opts &opts,
                         list &fetching)
{
	while(!fetching.empty() && !ctx::interruption_requested())
	{
		auto next
		{
			ctx::when_any(std::begin(fetching), std::end(fetching), []
			(auto &it) -> ctx::future<m::fetch::result> &
			{
				return it->future;
			})
		};

		if(!next.wait(milliseconds(50), std::nothrow))
			return true;

		const auto it
		{
			next.get()
		};

		assert(it != std::end(fetching));
		if(unlikely(it == std::end(fetching)))
			continue;

		const unique_iterator erase
		{
			fetching, it
		};

		auto _opts(opts);
		_opts.room.event_id = it->event_id;
		handle(_opts, it->future);
	}

	return false;
}

bool
ircd::m::acquire::handle(const opts &opts,
                         ctx::future<m::fetch::result> &future)
try
{
	auto result
	{
		future.get()
	};

	const json::object response
	{
		result
	};

	const json::array pdus
	{
		response["pdus"]
	};

	const m::event event
	{
		pdus.at(0), opts.room.event_id
	};

	m::vm::opts vmopts;
	vmopts.infolog_accept = true;
	vmopts.phase.set(m::vm::phase::FETCH_PREV, false);
	vmopts.phase.set(m::vm::phase::FETCH_STATE, false);
	vmopts.warnlog &= ~vm::fault::EXISTS;
	vmopts.notify_servers = false;
	m::vm::eval
	{
		event, vmopts
	};

	return true;
}
catch(const ctx::interrupted &e)
{
	throw;
}
catch(const std::exception &e)
{
	log::error
	{
		log, "Acquire %s in %s :%s",
		string_view{opts.room.event_id},
		string_view{opts.room.room_id},
		e.what(),
	};

	return false;
}
