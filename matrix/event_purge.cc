// Matrix Construct
//
// Copyright (C) Matrix Construct Developers, Authors & Contributors
// Copyright (C) 2016-2023 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

ircd::m::event::purge::purge(const event::idx &event_idx)
:purge
{
	event_idx, dbs::opts
	{
	}
}
{
}

ircd::m::event::purge::purge(const event::idx &event_idx,
                             dbs::opts opts)
:returns{false}
{
	assert(m::dbs::events);
	db::txn txn
	{
		*m::dbs::events
	};

	if(!purge(txn, event_idx, opts))
		return;

	txn();
	ret = true;
}

ircd::m::event::purge::purge(db::txn &txn,
                             const event::idx &event_idx)
:purge
{
	txn, event_idx, dbs::opts
	{
	}
}
{
}

ircd::m::event::purge::purge(db::txn &txn,
                             const event::idx &event_idx,
                             dbs::opts opts)
:returns{false}
{
	const m::event::fetch event
	{
		std::nothrow, event_idx
	};

	if(!event.valid)
		return;

	ret = purge(txn, event_idx, event, opts);
}

ircd::m::event::purge::purge(db::txn &txn,
                             const event::idx &event_idx,
                             const event &event)
:purge
{
	txn, event_idx, event, dbs::opts
	{
	}
}
{
}

ircd::m::event::purge::purge(db::txn &txn,
                             const event::idx &event_idx,
                             const event &event,
                             dbs::opts opts)
:returns{false}
{
	opts.op = db::op::DELETE;
	opts.event_idx = event_idx;
	m::dbs::write(txn, event, opts);
	ret = true;
}
