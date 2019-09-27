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

bool
ircd::m::event::horizon::has(const event::id &event_id)
{
	char buf[m::dbs::EVENT_HORIZON_KEY_MAX_SIZE];
	const string_view &key
	{
		m::dbs::event_horizon_key(buf, event_id, 0UL)
	};

	auto it
	{
		m::dbs::event_horizon.begin(key)
	};

	return bool(it);
}

size_t
ircd::m::event::horizon::count()
const
{
	size_t ret(0);
	for_each([&ret]
	(const auto &, const auto &)
	{
		++ret;
		return true;
	});

	return ret;
}

bool
ircd::m::event::horizon::has(const event::idx &event_idx)
const
{
	return !for_each([&event_idx]
	(const auto &, const auto &_event_idx)
	{
		// false to break; true to continue.
		return _event_idx == event_idx? false : true;
	});
}

bool
ircd::m::event::horizon::for_each(const closure_bool &closure)
const
{
	if(!this->event_id)
		return for_every(closure);

	char buf[m::dbs::EVENT_HORIZON_KEY_MAX_SIZE];
	const string_view &key
	{
		m::dbs::event_horizon_key(buf, event_id, 0UL)
	};

	for(auto it(m::dbs::event_horizon.begin(key)); it; ++it)
	{
		const auto &event_idx
		{
			std::get<0>(m::dbs::event_horizon_key(it->first))
		};

		if(!closure(event_id, event_idx))
			return false;
	}

	return true;
}

bool
ircd::m::event::horizon::for_every(const closure_bool &closure)
{
	db::column &column{dbs::event_horizon};
	for(auto it(column.begin()); it; ++it)
	{
		const auto &parts
		{
			split(it->first, "\0"_sv)
		};

		const auto &event_id
		{
			parts.first
		};

		const auto &event_idx
		{
			byte_view<event::idx>(parts.second)
		};

		if(!closure(event_id, event_idx))
			return false;
	}

	return true;
}
