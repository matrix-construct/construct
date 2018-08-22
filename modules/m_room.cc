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
purge(const m::room &room)
{
	size_t ret(0);
	db::txn txn
	{
		*m::dbs::events
	};

	room.for_each([&txn, &ret]
	(const m::event::idx &idx)
	{
		const m::event::fetch event
		{
			idx
		};

		m::dbs::write_opts opts;
		opts.op = db::op::DELETE;
		opts.event_idx = idx;
		m::dbs::write(txn, event, opts);
		++ret;
	});

	txn();
	return ret;
}

extern "C" void
make_auth(const m::room &room,
          json::stack::array &out,
          const vector_view<const string_view> &types,
          const string_view &member)
{
	const m::room::state state
	{
		room
	};

	const auto fetch{[&out, &state]
	(const string_view &type, const string_view &state_key)
	{
		state.get(std::nothrow, type, state_key, m::event::id::closure{[&out]
		(const auto &event_id)
		{
			json::stack::array auth{out};
			auth.append(event_id);
			{
				json::stack::object hash{auth};
				json::stack::member will
				{
					hash, "", ""
				};
			}
		}});
	}};

	for(const auto &type : types)
		fetch(type, "");

	if(member)
		fetch("m.room.member", member);
}

extern "C" json::array
make_auth__buf(const m::room &room,
               const mutable_buffer &buf,
               const vector_view<const string_view> &types,
               const string_view &member)
{
	json::stack ps{buf};
	{
		json::stack::array top{ps};
		make_auth(room, top, types, member);
	}

	return json::array{ps.completed()};
}

extern "C" int64_t
make_prev(const m::room &room,
          json::stack::array &out,
          size_t limit,
          bool need_tophead = true)
{
	const auto top_head
	{
		need_tophead?
			m::top(std::nothrow, room.room_id):
			std::tuple<m::id::event::buf, int64_t, m::event::idx>{}
	};

	int64_t depth{-1};
	m::event::fetch event;
	const m::room::head head{room};
	head.for_each(m::room::head::closure_bool{[&]
	(const m::event::idx &idx, const m::event::id &event_id)
	{
		seek(event, idx, std::nothrow);
		if(!event.valid)
			return true;

		if(need_tophead)
			if(json::get<"event_id"_>(event) == std::get<0>(top_head))
				need_tophead = false;

		depth = std::max(json::get<"depth"_>(event), depth);
		json::stack::array prev{out};
		prev.append(event_id);
		{
			json::stack::object hash{prev};
			json::stack::member will
			{
				hash, "", ""
			};
		}

		return --limit - need_tophead > 0;
	}});

	if(need_tophead)
	{
		depth = std::get<1>(top_head);
		json::stack::array prev{out};
		prev.append(std::get<0>(top_head));
		{
			json::stack::object hash{prev};
			json::stack::member will
			{
				hash, "", ""
			};
		}
	}

	return depth;
}

extern "C" std::pair<json::array, int64_t>
make_prev__buf(const m::room &room,
               const mutable_buffer &buf,
               const size_t &limit,
               const bool &need_tophead)
{
	int64_t depth;
	json::stack ps{buf};
	{
		json::stack::array top{ps};
		depth = make_prev(room, top, limit, need_tophead);
	}

	return
	{
		json::array{ps.completed()}, depth
	};
}

extern "C" std::pair<bool, int64_t>
is_complete(const m::room &room)
{
	const m::room::state state
	{
		room
	};

	const auto create_id
	{
		state.get("m.room.create")
	};

	static const m::event::fetch::opts fopts
	{
		m::event::keys::include{"depth"},
		{ db::get::NO_CACHE }
	};

	m::room::messages it
	{
		room, create_id, &fopts
	};

	int64_t depth{0};
	if(!it)
		return {false, depth};

	for(; it; ++it)
	{
		const m::event &event{*it};
		if(at<"depth"_>(event) == depth + 1)
			++depth;

		if(at<"depth"_>(event) != depth)
			return {false, depth};
	}

	return {true, depth};
}

extern "C" bool
state__force_present(const m::event &event)
{
	db::txn txn
	{
		*m::dbs::events
	};

	if(!defined(json::get<"state_key"_>(event)))
		throw error
		{
			"event %s is not a state event (no state_key)",
			json::get<"event_id"_>(event)
		};

	m::dbs::write_opts opts;
	opts.event_idx = index(event);
	opts.present = true;
	opts.history = false;
	opts.head = false;
	opts.refs = false;

	m::dbs::_index__room_state(txn, event, opts);
	m::dbs::_index__room_joined(txn, event, opts);

	txn();
	return true;
}

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

	static const m::event::fetch::opts fopts
	{
		{ db::get::NO_CACHE }
	};

	m::room::messages it
	{
		room, create_id, &fopts
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

	static const m::event::fetch::opts fopts
	{
		{ db::get::NO_CACHE }
	};

	m::room::messages it
	{
		room, create_id, &fopts
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

	static const m::event::fetch::opts fopts
	{
		{ db::get::NO_CACHE }
	};

	m::room::messages it
	{
		room, create_id, &fopts
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

extern "C" void
head__modify(const m::event::id &event_id,
             const db::op &op,
             const bool &refs)
{
	const m::event::fetch event
	{
		event_id
	};

	db::txn txn
	{
		*m::dbs::events
	};

	// Iterate all of the existing heads with a delete operation
	m::dbs::write_opts opts;
	opts.op = op;
	opts.head = true;
	opts.refs = refs;
	opts.event_idx = index(event);
	m::dbs::_index__room_head(txn, event, opts);

	// Commit txn
	txn();
}

extern "C" size_t
dagree_histogram(const m::room &room,
                 std::vector<size_t> &vec)
{
	static const m::event::fetch::opts fopts
	{
		{ db::get::NO_CACHE },
		m::event::keys::include
		{
			"event_id",
			"prev_events",
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

extern "C" void
room_herd(const m::room &room,
          const m::user &user,
          const milliseconds &timeout)
{
	using closure_prototype = bool (const string_view &,
	                                std::exception_ptr,
	                                const json::object &);

	using prototype = void (const m::room::id &,
	                        const m::user::id &,
	                        const milliseconds &,
	                        const std::function<closure_prototype> &);

	static m::import<prototype> feds__head
	{
		"federation_federation", "feds__head"
	};

	std::set<std::string> event_ids;
	feds__head(room.room_id, user.user_id, timeout, [&event_ids]
	(const string_view &origin, std::exception_ptr eptr, const json::object &event)
	{
		if(eptr)
			return true;

		const json::array prev_events
		{
			event.at("prev_events")
		};

		for(const json::array prev_event : prev_events)
		{
			const auto &prev_event_id
			{
				unquote(prev_event.at(0))
			};

			event_ids.emplace(prev_event_id);
		};

		return true;
	});

	ssize_t i(0);
	for(const m::event::id &event_id : event_ids)
		if(exists(event_id))
		{
			head__modify(event_id, db::op::SET, false);
			++i;
		}

	const m::room::head head
	{
		room
	};

	for(; i >= 0 && head.count() > 1; --i)
	{
		const auto eid
		{
			send(room, user, "ircd.room.revelation", json::object{})
		};

		ctx::sleep(seconds(2));
	}
}
