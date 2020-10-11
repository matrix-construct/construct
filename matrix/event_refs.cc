// Matrix Construct
//
// Copyright (C) Matrix Construct Developers, Authors & Contributors
// Copyright (C) 2016-2018 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

///////////////////////////////////////////////////////////////////////////////
//
// event/refs.h
//

void
ircd::m::event::refs::rebuild()
{
	static const size_t pool_size{96};
	static const size_t log_interval{8192};

	db::txn txn
	{
		*m::dbs::events
	};

	auto &column
	{
		dbs::event_json
	};

	auto it
	{
		column.begin()
	};

	ctx::dock dock;
	ctx::pool pool;
	pool.min(pool_size);

	size_t i(0), j(0);
	const ctx::uninterruptible::nothrow ui;
	for(; it; ++it)
	{
		if(ctx::interruption_requested())
			break;

		const m::event::idx event_idx
		{
			byte_view<m::event::idx>(it->first)
		};

		std::string event{it->second};
		pool([&txn, &dock, &i, &j, event(std::move(event)), event_idx]
		{
			m::dbs::write_opts wopts;
			wopts.event_idx = event_idx;
			wopts.appendix.reset();
			wopts.appendix.set(dbs::appendix::EVENT_REFS);
			m::dbs::write(txn, json::object{event}, wopts);

			if(++j % log_interval == 0) log::info
			{
				m::log, "Refs builder @%zu:%zu of %lu (@idx: %lu)",
				i,
				j,
				m::vm::sequence::retired,
				event_idx
			};

			if(j >= i)
				dock.notify_one();
		});

		++i;
	}

	dock.wait([&i, &j]
	{
		return i == j;
	});

	txn();
}

bool
ircd::m::event::refs::prefetch()
const
{
	return prefetch(dbs::ref(-1));
}

bool
ircd::m::event::refs::prefetch(const dbs::ref &type)
const
{
	if(unlikely(!idx))
		return false;

	// Allow -1 to iterate through all types by starting
	// the iteration at type value 0 and then ignoring the
	// type as a loop continue condition.
	const bool all_type
	{
		type == dbs::ref(uint8_t(-1))
	};

	const auto &_type
	{
		all_type? dbs::ref::NEXT : type
	};

	static_assert(uint8_t(dbs::ref::NEXT) == 0);
	char buf[dbs::EVENT_REFS_KEY_MAX_SIZE];
	const string_view key
	{
		dbs::event_refs_key(buf, idx, _type, 0)
	};

	return db::prefetch(dbs::event_refs, key);
}

size_t
ircd::m::event::refs::count()
const
{
	return count(dbs::ref(-1));
}

size_t
ircd::m::event::refs::count(const dbs::ref &type)
const
{
	size_t ret(0);
	for_each(type, [&ret]
	(const event::idx &, const dbs::ref &)
	{
		++ret;
		return true;
	});

	return ret;
}

bool
ircd::m::event::refs::has(const event::idx &idx)
const
{
	return has(dbs::ref(-1), idx);
}

bool
ircd::m::event::refs::has(const dbs::ref &type)
const
{
	return !for_each(type, [&type]
	(const event::idx &, const dbs::ref &ref)
	{
		assert(ref == type);
		return false;
	});
}

bool
ircd::m::event::refs::has(const dbs::ref &type,
                          const event::idx &idx)
const
{
	return !for_each(type, [&idx]
	(const event::idx &ref, const dbs::ref &)
	{
		return ref != idx; // true to continue, false to break
	});
}

bool
ircd::m::event::refs::for_each(const closure &closure)
const
{
	return for_each(dbs::ref(-1), closure);
}

bool
ircd::m::event::refs::for_each(const dbs::ref &type,
                               const closure &closure)
const
{
	if(!idx)
		return true;

	// Allow -1 to iterate through all types by starting
	// the iteration at type value 0 and then ignoring the
	// type as a loop continue condition.
	const bool all_type
	{
		type == dbs::ref(uint8_t(-1))
	};

	const auto &_type
	{
		all_type? dbs::ref::NEXT : type
	};

	static_assert(uint8_t(dbs::ref::NEXT) == 0);
	char buf[dbs::EVENT_REFS_KEY_MAX_SIZE];
	const string_view key
	{
		dbs::event_refs_key(buf, idx, _type, 0)
	};

	auto it
	{
		dbs::event_refs.begin(key)
	};

	for(; it; ++it)
	{
		const auto parts
		{
			dbs::event_refs_key(it->first)
		};

		const auto &type
		{
			std::get<0>(parts)
		};

		if(!all_type && type != _type)
			break;

		const auto &ref
		{
			std::get<1>(parts)
		};

		assert(idx != ref);
		if(!closure(ref, type))
			return false;
	}

	return true;
}
