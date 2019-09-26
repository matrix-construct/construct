// Matrix Construct
//
// Copyright (C) Matrix Construct Developers, Authors & Contributors
// Copyright (C) 2016-2019 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

//TODO: XXX remove fwd decl
namespace ircd::m::dbs
{
	void _index_event_horizon(db::txn &, const event &, const write_opts &, const m::event::id &);
}

size_t
ircd::m::event::horizon::rebuild()
{
	m::dbs::write_opts opts;
	opts.appendix.reset();
	opts.appendix.set(dbs::appendix::EVENT_HORIZON);
	db::txn txn
	{
		*dbs::events
	};

	size_t ret(0);
	m::events::for_each({0UL, -1UL}, [&ret, &txn, &opts]
	(const m::event::idx &event_idx, const m::event &event)
	{
		const m::event::prev prev
		{
			event
		};

		m::for_each(prev, [&ret, &txn, &opts, &event_idx, &event]
		(const m::event::id &event_id)
		{
			if(m::exists(event_id))
				return true;

			opts.event_idx = event_idx;
			m::dbs::_index_event_horizon(txn, event, opts, event_id);
			if(++ret % 1024 == 0)
				log::info
				{
					m::log, "event::horizon rebuild @ %lu/%lu",
					event_idx,
					m::vm::sequence::retired,
				};

			return true;
		});

		return true;
	});

	txn();
	return ret;
}
