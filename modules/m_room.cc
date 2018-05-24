// Matrix Construct
//
// Copyright (C) Matrix Construct Developers, Authors & Contributors
// Copyright (C) 2016-2018 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

using namespace ircd;

mapi::header
IRCD_MODULE
{
	"Matrix state library; modular components."
};

extern "C" size_t
state__rebuild_present(const m::room &room)
{
	size_t ret{0};
	const m::room::state state
	{
		room
	};

	const auto create_id
	{
		state.get("m.room.create")
	};

	m::room::messages it
	{
		room, create_id
	};

	if(!it)
		return ret;

	db::txn txn
	{
		*m::dbs::events
	};

	for(; it; ++it)
	{
		const m::event &event{*it};
		if(!defined(json::get<"state_key"_>(event)))
			continue;

		m::dbs::write_opts opts;
		opts.event_idx = it.event_idx();
		opts.present = true;
		opts.history = false;
		opts.head = false;
		opts.refs = false;

		m::dbs::_index__room_state(txn, event, opts);
		m::dbs::_index__room_joined(txn, event, opts);
		++ret;
	}

	txn();
	return ret;
}

extern "C" size_t
state__rebuild_history(const m::room &room)
{
	size_t ret{0};
	const m::room::state state
	{
		room
	};

	const auto create_id
	{
		state.get("m.room.create")
	};

	m::room::messages it
	{
		room, create_id
	};

	if(!it)
		return ret;

	db::txn txn
	{
		*m::dbs::events
	};

	uint r(0);
	char root[2][64] {0};
	m::dbs::write_opts opts;
	opts.root_in = root[++r % 2];
	opts.root_out = root[++r % 2];
	opts.present = false;
	opts.history = true;
	opts.head = false;
	opts.refs = false;

	int64_t depth{0};
	for(; it; ++it)
	{
		const m::event &event{*it};
		opts.event_idx = it.event_idx();
		if(at<"depth"_>(event) == depth + 1)
			++depth;

		if(at<"depth"_>(event) != depth)
			throw ircd::error
			{
				"Incomplete room history: gap between %ld and %ld [%s]",
				depth,
				at<"depth"_>(event),
				string_view{at<"event_id"_>(event)}
			};

		if(at<"type"_>(event) == "m.room.redaction")
		{
			opts.root_in = m::dbs::_index_redact(txn, event, opts);
			opts.root_out = root[++r % 2];
			txn();
			txn.clear();
		}
		else if(defined(json::get<"state_key"_>(event)))
		{
			opts.root_in = m::dbs::_index_state(txn, event, opts);
			opts.root_out = root[++r % 2];
			txn();
			txn.clear();
		}
		else m::dbs::_index_ephem(txn, event, opts);

		++ret;
	}

	txn();
	return ret;
}

extern "C" size_t
head__rebuild(const m::room &room)
{
	size_t ret{0};
	const m::room::state state{room};
	const auto create_id
	{
		state.get("m.room.create")
	};

	m::room::messages it
	{
		room, create_id
	};

	if(!it)
		return ret;

	db::txn txn
	{
		*m::dbs::events
	};

	m::dbs::write_opts opts;
	opts.op = db::op::SET;
	opts.head = true;
	opts.refs = true;
	for(; it; ++it)
	{
		const m::event &event{*it};
		opts.event_idx = it.event_idx();
		m::dbs::_index__room_head(txn, event, opts);
		++ret;
	}

	txn();
	return ret;
}

extern "C" size_t
head__reset(const m::room &room)
{
	size_t ret{0};
	m::room::messages it
	{
		room
	};

	if(!it)
		return ret;

	// Replacement will be the single new head
	const m::event replacement
	{
		*it
	};

	db::txn txn
	{
		*m::dbs::events
	};

	// Iterate all of the existing heads with a delete operation
	m::dbs::write_opts opts;
	opts.op = db::op::DELETE;
	opts.head = true;
	m::room::head{room}.for_each([&room, &opts, &txn, &ret]
	(const m::event::idx &event_idx, const m::event::id &event_id)
	{
		const m::event::fetch event
		{
			event_idx, std::nothrow
		};

		if(!event.valid)
		{
			log::derror
			{
				"Invalid event '%s' idx %lu in head for %s",
				string_view{event_id},
				event_idx,
				string_view{room.room_id}
			};

			return;
		}

		opts.event_idx = event_idx;
		m::dbs::_index__room_head(txn, event, opts);
		++ret;
	});

	// Finally add the replacement to the txn
	opts.op = db::op::SET;
	opts.event_idx = it.event_idx();
	m::dbs::_index__room_head(txn, replacement, opts);

	// Commit txn
	txn();
	return ret;
}

extern "C" size_t
dagree_histogram(const m::room &room,
                 std::vector<size_t> &vec)
{
	static const m::event::fetch::opts fopts
	{
		m::event::keys::include
		{
			"event_id",
			"prev_events",
		},

		db::gopts
		{
			db::get::NO_CACHE
		}
	};

	m::room::messages it
	{
		room, &fopts
	};

	size_t ret{0};
	for(; it; --it)
	{
		const m::event event{*it};
		const size_t num{degree(event)};
		if(unlikely(num >= vec.size()))
		{
			log::warning
			{
				"Event '%s' had %zu prev events (ignored)",
				string_view(at<"event_id"_>(event))
			};

			continue;
		}

		++vec[num];
		++ret;
	}

	return ret;
}
