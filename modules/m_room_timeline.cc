// Matrix Construct
//
// Copyright (C) Matrix Construct Developers, Authors & Contributors
// Copyright (C) 2016-2019 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

ircd::mapi::header
IRCD_MODULE
{
	"Matrix room library; timeline modular components."
};

uint64_t
ircd::m::latency(const room::timeline &a,
                 const room::timeline &b)
{
	return 0UL;
}

//
// room::timeline::timeline
//

ircd::m::room::timeline::timeline(const m::room &room)
:room{room}
{
}

bool
ircd::m::room::timeline::for_each(const closure &closure,
                                  const coord &branch)
const
{
	m::event::refs refs
	{
		room.event_id?
			index(room.event_id):
			room::index(room)
	};

	if(!refs.idx)
		return true;

	timeline::coord coord;
	if(!closure(coord, refs.idx))
		return false;

	for(++coord.y; coord.y <= branch.y; ++coord.y, coord.x = 0)
	{
		auto idx(0);
		refs.for_each(dbs::ref::NEXT, [&coord, &branch, &idx]
		(const auto &event_idx, const auto &)
		{
			if(coord.x <= branch.x)
				idx = event_idx;

			if(coord.x < branch.x)
			{
				++coord.x;
				return true;
			}
			else return false;
		});

		if(!idx)
			return true;

		if(!closure(coord, idx))
			return false;

		refs.idx = idx;
	}

	return true;
}

bool
ircd::m::room::timeline::has_future(const event::id &event_id)
const
{
	return true;
}

bool
ircd::m::room::timeline::has_past(const event::id &event_id)
const
{

	return true;
}

void
ircd::m::room::timeline::rebuild(const m::room &room)
{
	m::room::messages it
	{
		room, 0UL
	};

	if(!it)
		return;

	db::txn txn
	{
		*m::dbs::events
	};

	for(; it; ++it)
	{
		const m::event &event{*it};
		m::dbs::write_opts opts;
		opts.event_idx = it.event_idx();
		opts.appendix.reset();
		opts.appendix.set(dbs::appendix::EVENT_REFS);
		opts.event_refs.reset();
		opts.event_refs.set(uint(dbs::ref::NEXT));
		m::dbs::write(txn, event, opts);
	}

	txn();
}
