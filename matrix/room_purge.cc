// Matrix Construct
//
// Copyright (C) Matrix Construct Developers, Authors & Contributors
// Copyright (C) 2016-2023 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

decltype(ircd::m::room::purge::opts_default)
ircd::m::room::purge::opts_default;

decltype(ircd::m::room::purge::log)
ircd::m::room::purge::log
{
	"m.room.purge"
};

ircd::m::room::purge::purge(const m::room &room,
                            const struct opts &opts)
:returns<size_t>
{
	0
}
,room
{
	room
}
,opts
{
	opts
}
,txn
{
	*dbs::events
}
{
	if(opts.timeline)
		timeline();
	else if(opts.state)
		state();

	commit();
}

void
ircd::m::room::purge::commit()
{
	assert(ret || !txn.size());
	if(!ret)
		return;

	if(opts.debuglog_txn || opts.infolog_txn)
		log::logf
		{
			log, opts.infolog_txn? log::level::INFO: log::level::DEBUG,
			"Purging %s events:%zu txn[cells:%zu bytes:%zu] opts[st:%b pr:%b hs:%b tl:%b depth[%lu:%lu]]",
			string_view{room.room_id},
			ret,
			txn.size(),
			txn.bytes(),
			opts.state,
			opts.present,
			opts.history,
			opts.timeline,
			opts.depth.first,
			opts.depth.second,
		};

	txn();
}

void
ircd::m::room::purge::state()
{
	const room::state::space space
	{
		room
	};

	space.for_each([this]
	(const auto &, const auto &, const auto &depth, const auto &event_idx)
	{
		if(!match(depth, event_idx))
			return true;

		const event::fetch event
		{
			std::nothrow, event_idx
		};

		if(unlikely(!event.valid))
			return true;

		if(!match(event_idx, event))
			return true;

		const auto purged
		{
			event::purge(txn, event_idx, event, opts.wopts)
		};

		ret += purged;
		return true;
	});
}

void
ircd::m::room::purge::timeline()
{
	room::events it
	{
		room, opts.depth.second
	};

	m::event::fetch event;
	for(; it && it.depth() >= opts.depth.first; --it)
	{
		if(!match(it.depth(), it.event_idx()))
			continue;

		if(unlikely(!seek(std::nothrow, event, it.event_idx())))
			continue;

		if(!match(it.event_idx(), event))
			continue;

		const bool purged
		{
			event::purge(txn, it.event_idx(), event, opts.wopts)
		};

		ret += purged;
	}
}

bool
ircd::m::room::purge::match(const uint64_t &depth,
                            const event::idx &event_idx)
const
{
	if(depth < opts.depth.first)
		return false;

	if(depth > opts.depth.second)
		return false;

	if(event_idx < opts.idx.first)
		return false;

	if(event_idx > opts.idx.second)
		return false;

	return true;
}

bool
ircd::m::room::purge::match(const event::idx &event_idx,
                            const event &event)
const
{
	if(!opts.timeline)
		if(!defined(json::get<"state_key"_>(event)))
			return false;

	if(!opts.state || (!opts.present && !opts.history))
		if(defined(json::get<"state_key"_>(event)))
			return false;

	if(opts.filter)
		if(!m::match(*opts.filter, event))
			return false;

	if(!opts.present)
		if(defined(json::get<"state_key"_>(event)))
			if(room::state::present(event_idx))
				return false;

	if(!opts.history)
		if(defined(json::get<"state_key"_>(event)))
			if(!room::state::present(event_idx))
				return false;

	return true;
}
