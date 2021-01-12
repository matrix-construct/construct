// The Construct
//
// Copyright (C) The Construct Developers, Authors & Contributors
// Copyright (C) 2016-2020 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

decltype(ircd::m::acquire::log)
ircd::m::acquire::log
{
	"m.acquire"
};

template<>
decltype(ircd::util::instance_list<ircd::m::acquire>::allocator)
ircd::util::instance_list<ircd::m::acquire>::allocator
{};

template<>
decltype(ircd::util::instance_list<ircd::m::acquire>::list)
ircd::util::instance_list<ircd::m::acquire>::list
{
	allocator
};

ircd::m::acquire::acquire::acquire(const struct opts &opts)
:opts{opts}
,head_vmopts{opts.vmopts}
,history_vmopts{opts.vmopts}
,state_vmopts{opts.vmopts}
{
	if(opts.head)
	{
		head_vmopts.notify_servers = false;
		head_vmopts.phase.set(m::vm::phase::NOTIFY, false);
		head_vmopts.phase.set(m::vm::phase::FETCH_PREV, false);
		head_vmopts.phase.set(m::vm::phase::FETCH_STATE, false);
	}

	if(opts.state)
	{
		state_vmopts.notify_servers = false;
		state_vmopts.phase.set(m::vm::phase::FETCH_PREV, false);
		state_vmopts.phase.set(m::vm::phase::FETCH_STATE, false);
		state_vmopts.wopts.appendix.set(dbs::appendix::ROOM_HEAD, false);
	}

	if(opts.history || opts.timeline)
	{
		history_vmopts.notify_servers = false;
		history_vmopts.phase.set(m::vm::phase::NOTIFY, false);
		history_vmopts.phase.set(m::vm::phase::FETCH_PREV, false);
		history_vmopts.phase.set(m::vm::phase::FETCH_STATE, false);
		history_vmopts.wopts.appendix.set(dbs::appendix::ROOM_HEAD, false);
	}

	// Branch to acquire head
	if(opts.head)
		if(!opts.depth.second)
			acquire_head();

	// Branch to acquire history
	if(opts.history)
		acquire_history();

	// Branch to acquire timeline
	if(opts.timeline)
		acquire_timeline();

	// Branch to acquire state
	if(opts.state)
		acquire_state();
}

ircd::m::acquire::~acquire()
noexcept try
{
	// Complete all work before returning, otherwise everything
	// will be cancelled on unwind.
	while(!fetching.empty())
		while(handle());
}
catch(const ctx::interrupted &)
{
	return;
}
catch(const ctx::terminated &)
{
	return;
}

void
ircd::m::acquire::acquire_history()
{
	event::idx ref_min
	{
		opts.ref.first
	};

	for(size_t i(0); i < opts.rounds; ++i)
	{
		if(!fetch_history(ref_min))
			break;

		if(ref_min > opts.ref.second)
			break;
	}
}

bool
ircd::m::acquire::fetch_history(event::idx &ref_min)
{
	const auto top
	{
		m::top(opts.room.room_id)
	};

	const auto &[top_id, top_depth, top_idx]
	{
		top
	};

	auto depth_range
	{
		opts.depth
	};

	if(!depth_range.first && opts.viewport_size)
		depth_range =
		{
			m::viewport(opts.room).first, depth_range.second
		};

	if(!depth_range.second)
		depth_range.second = top_depth;

	if(size_t(depth_range.second - depth_range.first) < opts.viewport_size)
		depth_range.first -= std::min(long(opts.viewport_size), depth_range.first);

	m::room::events::missing missing
	{
		opts.room
	};

	bool ret(false);
	event::idx ref_top(ref_min);
	missing.for_each(depth_range, [this, &top, &ref_min, &ref_top, &ret]
	(const event::id &event_id, const int64_t &ref_depth, const event::idx &ref_idx)
	{
		if(ctx::interruption_requested())
			return false;

		if(ref_idx < opts.ref.first || ref_idx < ref_min)
			return true;

		if(ref_idx > opts.ref.second)
			return true;

		// Branch if we have to measure the viewportion
		if(opts.viewport_size)
		{
			const m::event::idx_range idx_range
			{
				std::min(ref_idx, std::get<event::idx>(top)),
				std::max(ref_idx, std::get<event::idx>(top)),
			};

			// Bail if this event sits above the viewport.
			if(m::room::events::count(opts.room, idx_range) > opts.viewport_size)
				return true;
		}

		const auto ref_id
		{
			m::event_id(ref_idx)
		};

		const m::room ref_room
		{
			opts.room.room_id, ref_id
		};

		const auto &[sound_depth, sound_idx]
		{
			m::sounding(ref_room)
		};

		const auto &[twain_depth, _twain_idx]
		{
			sound_idx == ref_idx?
				m::twain(ref_room):
				std::make_pair(0L, 0UL)
		};

		const auto gap
		{
			sound_depth >= twain_depth?
				size_t(sound_depth - twain_depth):
				0UL
		};

		// Ignore if this ref borders on a gap which does not satisfy the options
		if(gap < opts.gap.first || gap > opts.gap.second)
			return true;

		// The depth on each side of a gap is used as a poor heuristic to
		// guesstimate how many events might be missing and how much to
		// request from a remote at once. Due to protocol limitations, this
		// can err in both directions:
		// - It lowballs in situations like #ping:maunium.net where the DAG
		// is wide, causing more rounds of requests to fill a gap.
		// - It's overzealous in cases of secondary/distant references that
		// have nothing to do with a gap preceding the ref.
		//
		// Fortunately in practice the majority of estimates are close enough.
		// XXX /get_missing_events should be considered if there's low
		// confidence in a gap estimate.
		const auto &limit
		{
			std::clamp(gap, 1UL, 48UL)
		};

		const bool submitted
		{
			submit(event_id, opts.hint, false, limit, &history_vmopts)
		};

		if(submitted)
			log::debug
			{
				log, "Fetch %s miss prev of %s @%lu in %s @%lu sound:%lu twain:%ld fetching:%zu",
				string_view{event_id},
				string_view{ref_id},
				ref_depth,
				string_view{ref_room.room_id},
				std::get<int64_t>(top),
				sound_depth,
				twain_depth,
				fetching.size(),
			};

		ref_top = std::max(ref_top, ref_idx);
		ret |= submitted;
		return true;
	});

	assert(ref_top >= ref_min);
	ref_min = ref_top;
	return ret;
}

void
ircd::m::acquire::acquire_timeline()
{
	// We only enter this routine if the options are sufficient. Otherwise
	// the functionality here will overlap with acquire_history() and be
	// a more expensive form of the same thing.
	const bool sufficient_options
	{
		false
		|| !opts.history
		|| opts.viewport_size
		|| opts.depth.first
		|| opts.depth.second
	};

	if(!sufficient_options)
		return;

	event::idx ref_min
	{
		opts.ref.first
	};

	for(size_t i(0); i < opts.rounds; ++i)
	{
		if(!fetch_timeline(ref_min))
			break;

		if(ref_min > opts.ref.second)
			break;
	}
}

bool
ircd::m::acquire::fetch_timeline(event::idx &ref_min)
{
	bool ret(false);
	auto _ref_min(ref_min);
	std::set<event::idx> pe;
	std::deque<event::idx> pq;

	event::idx _event_idx;
	if(opts.room.event_id)
		if((_event_idx = m::index(std::nothrow, opts.room.event_id)))
			pq.emplace_back(_event_idx);

	if(pq.empty())
		m::room::head(opts.room).for_each([&pq]
		(const auto &event_idx, const auto &event_id)
		{
			pq.emplace_back(event_idx);
			return pq.size() < event::prev::MAX;
		});

	if(pq.empty())
		pq.emplace_back(m::head_idx(opts.room));

	size_t submits(0);
	size_t leaf_ctr(0);
	m::event::fetch e, p; do
	{
		const auto ref_idx
		{
			pq.front()
		};

		pq.pop_front();
		if(ref_idx < opts.ref.first || ref_idx < ref_min)
			continue;

		if(ref_idx > opts.ref.second)
			continue;

		if(!seek(std::nothrow, e, ref_idx))
			continue;

		const event::prev prev{e};
		event::id _prev_id[prev.MAX];
		const auto &prev_id
		{
			prev.ids(_prev_id)
		};

		assert(prev_id.size() <= prev.MAX);
		event::idx _prev_idx[prev.MAX];
		const auto prev_idx
		{
			prev.idxs(_prev_idx)
		};

		size_t fetched(0);
		for(size_t i(0); i < prev_idx.size(); ++i)
		{
			if(prev_idx[i])
				continue;

			const bool submitted
			{
				submit(prev_id[i], opts.hint, false, 1, &history_vmopts)
			};

			if(!submitted)
				continue;

			log::debug
			{
				log, "Fetch from %s (%lu) miss prev %s fetch:%zu in %s pe:%zu pq:%zu fetching:%zu",
				string_view{e.event_id},
				ref_idx,
				string_view{prev_id[i]},
				fetched,
				string_view{opts.room.room_id},
				pe.size(),
				pq.size(),
				fetching.size(),
			};

			++fetched;
			++submits;
			ret |= true;
		}

		if(pq.size() >= (opts.leaf_depth?: prev.MAX))
			continue;

		if(opts.leaf_depth || opts.viewport_size)
		{
			if(prev_id.size() == 1)
			{
				if(++leaf_ctr % (opts.viewport_size?: opts.leaf_depth) == 0)
					continue;
			}
			else leaf_ctr = 0;
		}

		size_t pushed(0);
		for(size_t i(0); i < prev_idx.size(); ++i)
		{
			if(!prev_idx[i])
				continue;

			auto it(pe.lower_bound(prev_idx[i]));
			if(it != end(pe) && *it == prev_idx[i])
				continue;

			pe.emplace_hint(it, prev_idx[i]);
			if(opts.depth.first || opts.depth.second)
			{
				const auto depth
				{
					m::get(std::nothrow, prev_idx[i], "depth", 0L)
				};

				if(depth < opts.depth.first)
					continue;

				if(opts.depth.second && depth > opts.depth.second)
					continue;
			}

			pq.emplace_back(prev_idx[i]);
			_ref_min = std::max(_ref_min, prev_idx[i]);
			++pushed;
			log::debug
			{
				log, "Queue from %s (%lu) next prev %s (%lu) push:%zu in %s pe:%zu pq:%zu fetching:%zu",
				string_view{e.event_id},
				ref_idx,
				string_view{prev_id[i]},
				prev_idx[i],
				pushed,
				string_view{opts.room.room_id},
				pe.size(),
				pq.size(),
				fetching.size(),
			};
		}
	}
	while(!pq.empty() && submits < opts.fetch_max);

	log::debug
	{
		log, "Round in %s pe:%zu pq:%zu submits:%zu fetching:%zu ref_min:%lu:%lu",
		string_view{opts.room.room_id},
		pe.size(),
		pq.size(),
		submits,
		fetching.size(),
		ref_min,
		_ref_min,
	};

	ref_min = std::max(ref_min, _ref_min);
	return ret;
}

void
ircd::m::acquire::acquire_state()
{
	m::event::id::buf event_id;
	if(opts.room.event_id)
		event_id = opts.room.event_id;

	if(!event_id && opts.viewport_size)
		event_id = m::event_id(std::nothrow, m::viewport(opts.room).second); //TODO: opts.viewport_size

	if(!event_id && opts.history)
		event_id = m::event_id(std::nothrow, m::sounding(opts.room).second);

	if(!event_id && opts.head)
		event_id = m::head(opts.room);

	if(!event_id)
		return;

	m::room::state::fetch::opts sfopts;
	sfopts.room.room_id = opts.room.room_id;
	sfopts.room.event_id = event_id;
	m::room::state::fetch
	{
		sfopts, [this](const m::event::id &event_id, const string_view &remote)
		{
			return fetch_state(event_id, remote);
		}
	};
}

bool
ircd::m::acquire::fetch_state(const m::event::id &event_id,
                              const string_view &remote)
{
	// Bail if interrupted
	if(ctx::interruption_requested())
		return false;

	const auto hint
	{
		event_id.host() && !my_host(event_id.host())?
			event_id.host():

		remote && !my_host(remote)?
			remote:

		opts.room.room_id.host() && !my_host(opts.room.room_id.host())?
			opts.room.room_id.host():

		string_view{}
	};

	const bool submitted
	{
		submit(event_id, hint, false, 1, &state_vmopts)
	};

	if(submitted)
		log::debug
		{
			log, "Fetch %s in state of %s fetching:%zu",
			string_view{event_id},
			string_view{opts.room.room_id},
			fetching.size(),
		};

	return true;
}

void
ircd::m::acquire::acquire_head()
{
	m::room::head::fetch::opts hfopts;
	hfopts.room_id = opts.room.room_id;
	hfopts.top = m::top(opts.room.room_id);
	m::room::head::fetch
	{
		hfopts, [this, &hfopts](const m::event &result)
		{
			const auto &[top_id, top_depth, top_idx]
			{
				hfopts.top
			};

			return fetch_head(result, top_depth);
		}
	};
}

bool
ircd::m::acquire::fetch_head(const m::event &result,
                             const int64_t &top_depth)
{
	// Bail if interrupted
	if(ctx::interruption_requested())
		return false;

	// Bail if the depth is below the window
	if(json::get<"depth"_>(result) < opts.depth.first)
		return false;

	const auto gap
	{
		json::get<"depth"_>(result) - top_depth
	};

	const auto &limit
	{
		std::clamp(gap, 1L, 48L)
	};

	const auto &hint
	{
		json::get<"origin"_>(result)
	};

	const bool submitted
	{
		submit(result.event_id, hint, true, limit, &head_vmopts)
	};

	if(submitted)
		log::debug
		{
			log, "Fetch %s head from '%s' in %s @%lu fetching:%zu",
			string_view{result.event_id},
			hint,
			string_view{opts.room.room_id},
			top_depth,
			fetching.size(),
		};

	return true;
}

bool
ircd::m::acquire::submit(const m::event::id &event_id,
                         const string_view &hint,
                         const bool &hint_only,
                         const size_t &limit,
                         const vm::opts *const &vmopts)
{
	const bool ret
	{
		!started(event_id)?
			start(event_id, hint, hint_only, limit, vmopts):
			false
	};

	if(ret || full())
		while(handle());

	return ret;
}

bool
ircd::m::acquire::start(const m::event::id &event_id,
                        const string_view &hint,
                        const bool &hint_only,
                        const size_t &limit,
                        const vm::opts *const &vmopts)
try
{
	assert(vmopts);

	fetch::opts fopts;
	fopts.room_id = opts.room.room_id;
	fopts.event_id = event_id;
	fopts.backfill_limit = limit;
	fopts.op =
		(limit > 1 || hint)?
			fetch::op::backfill:
			fetch::op::event;

	fopts.hint = hint;
	fopts.attempt_limit =
		!hint_only?
			opts.attempt_max:
			1U;

	fetching.emplace_back(result
	{
		vmopts, fetch::start(fopts), event_id
	});

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
		log, "Fetch %s in %s from '%s' :%s",
		string_view{event_id},
		string_view{opts.room.room_id},
		hint?: "<any>"_sv,
		e.what(),
	};

	return false;
}

bool
ircd::m::acquire::started(const event::id &event_id)
const
{
	const auto it
	{
		std::find_if(std::begin(fetching), std::end(fetching), [&event_id]
		(const auto &result)
		{
			return result.event_id == event_id;
		})
	};

	return it != std::end(fetching);
}

bool
ircd::m::acquire::handle()
{
	if(fetching.empty())
		return false;

	auto next
	{
		ctx::when_any(std::begin(fetching), std::end(fetching), []
		(auto &it) -> ctx::future<m::fetch::result> &
		{
			return it->future;
		})
	};

	const milliseconds timeout
	{
		full()? 5000: 50
	};

	ctx::interruption_point();
	if(!next.wait(timeout, std::nothrow))
		return full();

	const unique_iterator it
	{
		fetching, next.get()
	};

	assert(it.it != std::end(fetching));
	return handle(*it.it);
}

bool
ircd::m::acquire::handle(result &result)
try
{
	auto response
	{
		result.future.get()
	};

	const json::object body
	{
		response
	};

	const json::array pdus
	{
		body["pdus"]
	};

	log::debug
	{
		log, "Eval %zu from '%s' for %s in %s",
		pdus.size(),
		string_view{response.origin},
		string_view{result.event_id},
		string_view{opts.room.room_id},
	};

	assert(result.vmopts);
	assert
	(
		false
		|| result.vmopts == &this->head_vmopts
		|| result.vmopts == &this->history_vmopts
		|| result.vmopts == &this->state_vmopts
	);

	auto vmopts(*result.vmopts);
	vmopts.node_id = response.origin;

	m::vm::eval
	{
		pdus, vmopts
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
		log, "Eval %s in %s :%s",
		string_view{result.event_id},
		string_view{opts.room.room_id},
		e.what(),
	};

	return true;
}

bool
ircd::m::acquire::full()
const noexcept
{
	return fetching.size() >= opts.fetch_width;
}
