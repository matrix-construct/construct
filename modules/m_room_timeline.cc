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
ircd::m::room::timeline::for_each(const closure &closure)
const
{
	struct coord coord;
	return for_each(coord, closure);
}

bool
ircd::m::room::timeline::for_each(coord &coord,
                                  const closure &closure)
const
{
	messages it
	{
		this->room, uint64_t(coord.y)
	};

	if(!it)
		return true;

	event::idx next(it.event_idx()); do
	{
		const auto last(coord);
		coord = closure(coord, next);
		if(coord.x == last.x && coord.y == last.y)
			return true;

		if(coord.y != last.y)
			if(!it.seek(coord.y))
				return true;

		next = timeline::next(it.event_idx(), coord.x);
		if(next == 0 && coord.x == 0)
			continue;

		if(next == it.event_idx())
			return false;

		if(next == 0)
			coord.x = 0;
	}
	while(1);

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

//
// static util
//

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

ircd::m::event::idx
ircd::m::room::timeline::next(const event::idx &event_idx,
                              const int64_t &x)
{
	const m::event::refs refs
	{
		event_idx
	};

	int64_t _x(0);
	event::idx ret(0);
	refs.for_each(dbs::ref::NEXT, [&_x, &x, &ret]
	(const auto &event_idx, const auto &)
	{
		if(_x++ < x)
			return true;

		ret = event_idx;
		return false;
	});

	return ret;
}
