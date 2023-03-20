// Matrix Construct
//
// Copyright (C) Matrix Construct Developers, Authors & Contributors
// Copyright (C) 2016-2023 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

decltype(ircd::m::vm::sequence::dock)
ircd::m::vm::sequence::dock;

decltype(ircd::m::vm::sequence::retired)
ircd::m::vm::sequence::retired;

decltype(ircd::m::vm::sequence::committed)
ircd::m::vm::sequence::committed;

decltype(ircd::m::vm::sequence::uncommitted)
ircd::m::vm::sequence::uncommitted;

//
// refresh::refresh
//

ircd::m::vm::sequence::refresh::refresh()
{
	auto &database
	{
		db::database::get("events")
	};

	if(!database.slave)
		return;

	this->database[0] = db::sequence(database);
	this->retired[0] = sequence::retired;

	db::refresh(database);
	sequence::retired = sequence::get(this->event_id);

	this->database[1] = db::sequence(database);
	this->retired[1] = sequence::retired;
}

//
// tools
//

uint64_t
ircd::m::vm::sequence::min()
{
	const auto *const e
	{
		eval::seqmin()
	};

	return e? get(*e) : 0;
}

uint64_t
ircd::m::vm::sequence::max()
{
	const auto *const e
	{
		eval::seqmax()
	};

	return e? get(*e) : 0;
}

uint64_t
ircd::m::vm::sequence::get(id::event::buf &event_id)
{
	static constexpr auto column_idx
	{
		json::indexof<event, "event_id"_>()
	};

	auto &column
	{
		dbs::event_column.at(column_idx)
	};

	const auto it
	{
		column.rbegin()
	};

	if(!it)
	{
		// If this iterator is invalid the events db should
		// be completely fresh.
		assert(db::sequence(*dbs::events) == 0);
		return 0;
	}

	const auto &ret
	{
		byte_view<uint64_t>(it->first)
	};

	event_id = it->second;
	return ret;
}

const uint64_t &
ircd::m::vm::sequence::get(const eval &eval)
{
	return eval.sequence;
}
