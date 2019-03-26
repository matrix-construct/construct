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
	"Matrix room library; modular components."
};

ircd::m::event::id::buf
IRCD_MODULE_EXPORT
ircd::m::send(const m::room &room,
              const m::id::user &sender,
              const string_view &type,
              const json::iov &content)
{
	json::iov event;
	const json::iov::push push[]
	{
		{ event,    { "sender",  sender  }},
		{ event,    { "type",    type    }},
	};

	return commit(room, event, content);
}

m::event::id::buf
IRCD_MODULE_EXPORT
ircd::m::send(const m::room &room,
              const m::id::user &sender,
              const string_view &type,
              const string_view &state_key,
              const json::iov &content)
{
	json::iov event;
	const json::iov::push push[]
	{
		{ event,    { "sender",     sender     }},
		{ event,    { "type",       type       }},
		{ event,    { "state_key",  state_key  }},
	};

	return commit(room, event, content);
}

size_t
IRCD_MODULE_EXPORT
ircd::m::count_since(const m::room &room,
                     const m::event::idx &a,
                     const m::event::idx &b)
{
	m::room::messages it
	{
		room
	};

	assert(a <= b);
	it.seek_idx(a);

	if(!it && !exists(room))
		throw m::NOT_FOUND
		{
			"Cannot find room '%s' to count events in",
			string_view{room.room_id}
		};
	else if(!it)
		throw m::NOT_FOUND
		{
			"Event @ idx:%lu or idx:%lu not found in room '%s' or at all",
			a,
			b,
			string_view{room.room_id}
		};

	size_t ret{0};
	// Hit the iterator once first otherwise the count will always increment
	// to `1` erroneously when it ought to show `0`.
	for(++it; it && it.event_idx() <= b; ++it, ++ret);
	return ret;
}

ircd::json::object
IRCD_MODULE_EXPORT
ircd::m::room::power::default_content(const mutable_buffer &buf,
                                      const m::user::id &creator)
{
	json::stack out{buf};
	json::stack::object content{out};

	assert(default_power_level == 50);
	json::stack::member
	{
		content, "ban", json::value(default_power_level)
	};

	json::stack::object
	{
		content, "events"
	};

	assert(default_event_level == 0);
	json::stack::member
	{
		content, "events_default", json::value(default_event_level)
	};

	json::stack::member
	{
		content, "invite", json::value(default_power_level)
	};

	json::stack::member
	{
		content, "kick", json::value(default_power_level)
	};

	{
		json::stack::object notifications
		{
			content, "notifications"
		};

		json::stack::member
		{
			notifications, "room", json::value(default_power_level)
		};
	}

	json::stack::member
	{
		content, "redact", json::value(default_power_level)
	};

	json::stack::member
	{
		content, "state_default", json::value(default_power_level)
	};

	{
		json::stack::object users
		{
			content, "users"
		};

		assert(default_creator_level == 100);
		json::stack::member
		{
			users, creator, json::value(default_creator_level)
		};
	}

	assert(default_user_level == 0);
	json::stack::member
	{
		content, "users_default", json::value(default_user_level)
	};

	content.~object();
	return json::object{out.completed()};
}

bool
IRCD_MODULE_EXPORT
ircd::m::room::origins::random(const origins &origins,
                               const closure &view,
                               const closure_bool &proffer)
{
	bool ret{false};
	const size_t max
	{
		origins.count()
	};

	if(unlikely(!max))
		return ret;

	auto select
	{
		ssize_t(rand::integer(0, max - 1))
	};

	const closure_bool closure{[&proffer, &view, &select]
	(const string_view &origin)
	{
		if(select-- > 0)
			return true;

		// Test if this random selection is "ok" e.g. the callback allows the
		// user to test a blacklist for this origin. Skip to next if not.
		if(proffer && !proffer(origin))
		{
			++select;
			return true;
		}

		view(origin);
		return false;
	}};

	const auto iteration{[&origins, &closure, &ret]
	{
		ret = !origins.for_each(closure);
	}};

	// Attempt select on first iteration
	iteration();

	// If nothing was OK between the random int and the end of the iteration
	// then start again and pick the first OK.
	if(!ret && select >= 0)
		iteration();

	return ret;
}

size_t
IRCD_MODULE_EXPORT
ircd::m::room::purge(const room &room)
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

void
IRCD_MODULE_EXPORT
ircd::m::room::auth::make_refs(const auth &auth,
                               json::stack::array &out,
                               const types &types,
                               const user::id &user_id)
{
	const m::room::state state
	{
		auth.room
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

	if(user_id)
		fetch("m.room.member", user_id);
}

size_t
IRCD_MODULE_EXPORT
ircd::m::room::head::rebuild(const head &head)
{
	size_t ret{0};
	const auto &room{head.room};
	const m::room::state state{room};
	const auto create_idx
	{
		state.get("m.room.create")
	};

	static const m::event::fetch::opts fopts
	{
		{ db::get::NO_CACHE }
	};

	m::room::messages it
	{
		room, create_idx, &fopts
	};

	if(!it)
		return ret;

	db::txn txn
	{
		*m::dbs::events
	};

	m::dbs::write_opts opts;
	opts.op = db::op::SET;
	opts.room_head = true;
	opts.room_refs = true;
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

size_t
IRCD_MODULE_EXPORT
ircd::m::room::head::reset(const head &head)
{
	size_t ret{0};
	const auto &room{head.room};
	m::room::messages it{room};
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
	opts.room_head = true;
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
				m::log, "Invalid event '%s' idx %lu in head for %s",
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

void
IRCD_MODULE_EXPORT
ircd::m::room::head::modify(const m::event::id &event_id,
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
	opts.room_head = true;
	opts.room_refs = refs;
	opts.event_idx = event.event_idx;
	m::dbs::_index__room_head(txn, event, opts);

	// Commit txn
	txn();
}

int64_t
IRCD_MODULE_EXPORT
ircd::m::room::head::make_refs(const head &head,
                               json::stack::array &out,
                               const size_t &_limit,
                               const bool &_need_tophead)
{
	size_t limit{_limit};
	bool need_tophead{_need_tophead};
	const auto &room{head.room};
	const auto top_head
	{
		need_tophead?
			m::top(std::nothrow, room.room_id):
			std::tuple<m::id::event::buf, int64_t, m::event::idx>{}
	};

	int64_t depth{-1};
	m::event::fetch event;
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

bool
IRCD_MODULE_EXPORT
ircd::m::room::head::for_each(const head &head,
                              const closure_bool &closure)
{
	auto it
	{
		dbs::room_head.begin(head.room.room_id)
	};

	for(; it; ++it)
	{
		const event::id &event_id
		{
			dbs::room_head_key(it->first)
		};

		const event::idx &event_idx
		{
			byte_view<event::idx>{it->second}
		};

		if(!closure(event_idx, event_id))
			return false;
	}

	return true;
}

std::pair<bool, int64_t>
IRCD_MODULE_EXPORT
ircd::m::is_complete(const room &room)
{
	static const event::keys::include fkeys
	{
		"depth"
	};

	static const event::fetch::opts fopts
	{
		fkeys, { db::get::NO_CACHE }
	};

	const room::state state
	{
		room
	};

	const auto create_idx
	{
		state.get("m.room.create")
	};

	room::messages it
	{
		room, create_idx, &fopts
	};

	int64_t depth(-1);
	if(!it)
		return { false, depth };

	for(; it; ++it)
	{
		const event &event{*it};
		if(at<"depth"_>(event) == depth + 1)
			++depth;
		else if(depth < 0)
			depth = at<"depth"_>(event);

		if(at<"depth"_>(event) != depth)
			return { false, depth };
	}

	return { true, depth };
}

size_t
IRCD_MODULE_EXPORT
ircd::m::room::state::purge_replaced(const state &state)
{
	db::txn txn
	{
		*m::dbs::events
	};

	size_t ret(0);
	m::room::messages it
	{
		state.room_id, uint64_t(0)
	};

	if(!it)
		return ret;

	for(; it; ++it)
	{
		const m::event::idx &event_idx(it.event_idx());
		if(!m::get(std::nothrow, event_idx, "state_key", [](const auto &) {}))
			continue;

		if(!m::event::refs(event_idx).count(m::dbs::ref::STATE))
			continue;

		// TODO: erase event
	}

	return ret;
}

bool
IRCD_MODULE_EXPORT
ircd::m::room::state::force_present(const m::event &event)
{
	db::txn txn
	{
		*m::dbs::events
	};

	if(!defined(json::get<"room_id"_>(event)))
		throw error
		{
			"event %s is not a room event (no room_id)",
			json::get<"event_id"_>(event)
		};

	if(!defined(json::get<"state_key"_>(event)))
		throw error
		{
			"event %s is not a state event (no state_key)",
			json::get<"event_id"_>(event)
		};

	m::dbs::write_opts opts;
	opts.event_idx = m::index(event);
	opts.present = true;
	opts.history = false;
	opts.room_head = false;
	opts.room_refs = false;

	m::dbs::_index__room_state(txn, event, opts);
	m::dbs::_index__room_joined(txn, event, opts);

	txn();
	return true;
}

size_t
IRCD_MODULE_EXPORT
ircd::m::room::state::rebuild_present(const state &state)
{
	size_t ret{0};
	const auto create_idx
	{
		state.get("m.room.create")
	};

	static const m::event::fetch::opts fopts
	{
		{ db::get::NO_CACHE }
	};

	const m::room room
	{
		state.room_id, nullptr, state.fopts
	};

	m::room::messages it
	{
		room, create_idx, &fopts
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
		opts.room_head = false;
		opts.room_refs = false;

		m::dbs::_index__room_state(txn, event, opts);
		m::dbs::_index__room_joined(txn, event, opts);
		++ret;
	}

	txn();
	return ret;
}

size_t
IRCD_MODULE_EXPORT
ircd::m::room::state::rebuild_history(const state &state)
{
	size_t ret{0};
	const auto create_idx
	{
		state.get("m.room.create")
	};

	static const m::event::fetch::opts fopts
	{
		{ db::get::NO_CACHE }
	};

	const m::room room
	{
		state.room_id, nullptr, state.fopts
	};

	m::room::messages it
	{
		room, create_idx, &fopts
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
	opts.room_head = false;
	opts.room_refs = false;

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
		else m::dbs::_index_other(txn, event, opts);

		++ret;
	}

	txn();
	return ret;
}

//TODO: state btree.
size_t
IRCD_MODULE_EXPORT
ircd::m::room::state::clear_history(const state &state)
{
	static const db::gopts gopts
	{
		db::get::NO_CACHE
	};

	db::txn txn
	{
		*m::dbs::events
	};

	auto it
	{
		m::dbs::room_events.begin(state.room_id, gopts)
	};

	size_t ret{0};
	for(; it; ++it, ret++)
	{
		const auto pair
		{
			m::dbs::room_events_key(it->first)
		};

		const auto &depth{std::get<0>(pair)};
		const auto &event_idx{std::get<1>(pair)};
		thread_local char buf[m::dbs::ROOM_EVENTS_KEY_MAX_SIZE];
		const string_view key
		{
			m::dbs::room_events_key(buf, state.room_id, depth, event_idx)
		};

		db::txn::append
		{
			txn, m::dbs::room_events,
			{
				db::op::SET,
				key
			}
		};
	}

	txn();
	return ret;
}

conf::item<ulong>
state__prefetch__yield_modulus
{
	{ "name",     "ircd.m.room.state_prefetch.yield_modulus" },
	{ "default",  256L                                       },
};

size_t
IRCD_MODULE_EXPORT
ircd::m::room::state::prefetch(const state &state,
                               const string_view &type,
                               const event::idx_range &range)
{
	const m::event::fetch::opts &fopts
	{
		state.fopts?
			*state.fopts:
			m::event::fetch::default_opts
	};

	size_t ret{0};
	state.for_each(type, m::event::closure_idx{[&ret, &fopts, &range]
	(const m::event::idx &event_idx)
	{
		if(event_idx < range.first)
			return;

		if(range.second && event_idx > range.second)
			return;

		m::prefetch(event_idx, fopts);
		++ret;

		const ulong ym(state__prefetch__yield_modulus);
		if(ym && ret % ym == 0)
			ctx::yield();
	}});

	return ret;
}

ircd::m::event::idx
IRCD_MODULE_EXPORT
ircd::m::room::state::prev(const event::idx &event_idx)
{
	event::idx ret{0};
	prev(event_idx, [&ret]
	(const event::idx &event_idx)
	{
		if(event_idx > ret)
			ret = event_idx;

		return true;
	});

	return ret;
}

ircd::m::event::idx
IRCD_MODULE_EXPORT
ircd::m::room::state::next(const event::idx &event_idx)
{
	event::idx ret{0};
	next(event_idx, [&ret]
	(const event::idx &event_idx)
	{
		if(event_idx > ret)
			ret = event_idx;

		return true;
	});

	return ret;
}

bool
IRCD_MODULE_EXPORT
ircd::m::room::state::next(const event::idx &event_idx,
                           const event::closure_idx_bool &closure)
{
	const m::event::refs refs
	{
		event_idx
	};

	return refs.for_each(dbs::ref::STATE, [&closure]
	(const event::idx &event_idx, const dbs::ref &ref)
	{
		assert(ref == dbs::ref::STATE);
		return closure(event_idx);
	});
}

bool
IRCD_MODULE_EXPORT
ircd::m::room::state::prev(const event::idx &event_idx,
                           const event::closure_idx_bool &closure)
{
	const m::event::refs refs
	{
		event_idx
	};

	return refs.for_each(dbs::ref::PREV_STATE, [&closure]
	(const event::idx &event_idx, const dbs::ref &ref)
	{
		assert(ref == dbs::ref::PREV_STATE);
		return closure(event_idx);
	});
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
				m::log, "Event '%s' had %zu prev events (ignored)",
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

	static mods::import<prototype> feds__head
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

	size_t i(0);
	for(const m::event::id &event_id : event_ids)
		if(exists(event_id))
		{
			m::room::head::modify(event_id, db::op::SET, false);
			++i;
		}

	const m::room::head head
	{
		room
	};

	for(; i + 1 >= 1 && head.count() > 1; --i)
	{
		const auto eid
		{
			send(room, user, "ircd.room.revelation", json::object{})
		};

		ctx::sleep(seconds(2));
	}
}

bool
IRCD_MODULE_EXPORT
ircd::m::room::auth::for_each(const auth &a,
                              const closure_bool &closure)
{
	const m::room &room{a.room};
	const auto &event_id{room.event_id};
	if(!event_id)
		return false;

	return true;
}

size_t
IRCD_MODULE_EXPORT
__attribute__((noreturn))
ircd::m::room::stats::bytes_total(const m::room &room)
{
	throw m::UNSUPPORTED
	{
		"Not yet implemented."
	};
}

size_t
IRCD_MODULE_EXPORT
__attribute__((noreturn))
ircd::m::room::stats::bytes_total_compressed(const m::room &room)
{
	throw m::UNSUPPORTED
	{
		"Not yet implemented."
	};
}

size_t
IRCD_MODULE_EXPORT
ircd::m::room::stats::bytes_json(const m::room &room)
{
	size_t ret(0);
	for(m::room::messages it(room); it; --it)
	{
		const m::event::idx &event_idx
		{
			it.event_idx()
		};

		const byte_view<string_view> key
		{
			event_idx
		};

		static const db::gopts gopts
		{
			db::get::NO_CACHE
		};

		ret += db::bytes_value(m::dbs::event_json, key, gopts);
	}

	return ret;
}

size_t
IRCD_MODULE_EXPORT
__attribute__((noreturn))
ircd::m::room::stats::bytes_json_compressed(const m::room &room)
{
	throw m::UNSUPPORTED
	{
		"Not yet implemented."
	};
}
