// Matrix Construct
//
// Copyright (C) Matrix Construct Developers, Authors & Contributors
// Copyright (C) 2016-2019 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

decltype(ircd::m::room::state::enable_history)
ircd::m::room::state::enable_history
{
	{ "name",     "ircd.m.room.state.enable_history" },
	{ "default",  true                               },
};

decltype(ircd::m::room::state::readahead_size)
ircd::m::room::state::readahead_size
{
	{ "name",     "ircd.m.room.state.readahead_size" },
	{ "default",  0L                                 },
};

//
// room::state::state
//

ircd::m::room::state::state(const m::room &room,
                            const event::fetch::opts *const &fopts)
:room_id
{
	room.room_id
}
,event_id
{
	room.event_id?
		event::id::buf{room.event_id}:
		event::id::buf{}
}
,fopts
{
	fopts?
		fopts:
		room.fopts
}
{
}

bool
ircd::m::room::state::prefetch(const string_view &type)
const
{
	return prefetch(type, string_view{});
}

bool
ircd::m::room::state::prefetch(const string_view &type,
                               const string_view &state_key)
const
{
	if(!present())
	{
		const history history
		{
			room_id, event_id
		};

		return history.prefetch(type, state_key);
	}

	char buf[dbs::ROOM_STATE_KEY_MAX_SIZE];
	const auto &key
	{
		dbs::room_state_key(buf, room_id, type, state_key)
	};

	return db::prefetch(dbs::room_state, key);
}

ircd::m::event::idx
ircd::m::room::state::get(const string_view &type,
                          const string_view &state_key)
const
{
	event::idx ret;
	get(type, state_key, event::closure_idx{[&ret]
	(const event::idx &event_idx)
	{
		ret = event_idx;
	}});

	return ret;
}

ircd::m::event::idx
ircd::m::room::state::get(std::nothrow_t,
                          const string_view &type,
                          const string_view &state_key)
const
{
	event::idx ret{0};
	get(std::nothrow, type, state_key, event::closure_idx{[&ret]
	(const event::idx &event_idx)
	{
		ret = event_idx;
	}});

	return ret;
}

void
ircd::m::room::state::get(const string_view &type,
                          const string_view &state_key,
                          const event::closure &closure)
const
{
	get(type, state_key, event::closure_idx{[this, &closure]
	(const event::idx &event_idx)
	{
		const event::fetch event
		{
			event_idx, fopts? *fopts : event::fetch::default_opts
		};

		closure(event);
	}});
}

void
ircd::m::room::state::get(const string_view &type,
                          const string_view &state_key,
                          const event::id::closure &closure)
const
{
	get(type, state_key, event::closure_idx{[&]
	(const event::idx &idx)
	{
		if(!m::event_id(std::nothrow, idx, closure))
			throw m::NOT_FOUND
			{
				"(%s,%s) in %s idx:%lu event_id :not found",
				type,
				state_key,
				string_view{room_id},
				idx,
			};
	}});
}

void
ircd::m::room::state::get(const string_view &type,
                          const string_view &state_key,
                          const event::closure_idx &closure)
const try
{
	if(!present())
	{
		const history history
		{
			room_id, event_id
		};

		closure(history.get(type, state_key));
		return;
	}

	auto &column{dbs::room_state};
	char key[dbs::ROOM_STATE_KEY_MAX_SIZE];
	column(dbs::room_state_key(key, room_id, type, state_key), [&closure]
	(const string_view &value)
	{
		closure(byte_view<event::idx>(value));
	});
}
catch(const db::not_found &e)
{
	throw m::NOT_FOUND
	{
		"(%s,%s) in %s :%s",
		type,
		state_key,
		string_view{room_id},
		e.what()
	};
}

bool
ircd::m::room::state::get(std::nothrow_t,
                          const string_view &type,
                          const string_view &state_key,
                          const event::closure &closure)
const
{
	return get(std::nothrow, type, state_key, event::closure_idx{[this, &closure]
	(const event::idx &event_idx)
	{
		const event::fetch event
		{
			std::nothrow, event_idx, fopts? *fopts : event::fetch::default_opts
		};

		closure(event);
	}});
}

bool
ircd::m::room::state::get(std::nothrow_t,
                          const string_view &type,
                          const string_view &state_key,
                          const event::id::closure &closure)
const
{
	return get(std::nothrow, type, state_key, event::closure_idx{[&closure]
	(const event::idx &idx)
	{
		m::event_id(std::nothrow, idx, closure);
	}});
}

bool
ircd::m::room::state::get(std::nothrow_t,
                          const string_view &type,
                          const string_view &state_key,
                          const event::closure_idx &closure)
const
{
	if(!present())
	{
		const history history
		{
			room_id, event_id
		};

		const auto event_idx
		{
			history.get(std::nothrow, type, state_key)
		};

		if(event_idx)
		{
			closure(event_idx);
			return true;
		}
		else return false;
	}

	auto &column{dbs::room_state};
	char key[dbs::ROOM_STATE_KEY_MAX_SIZE];
	return column(dbs::room_state_key(key, room_id, type, state_key), std::nothrow, [&closure]
	(const string_view &value)
	{
		closure(byte_view<event::idx>(value));
	});
}

bool
ircd::m::room::state::has(const event::idx &event_idx)
const
{
	static const event::fetch::opts fopts
	{
		event::keys::include { "type", "state_key" },
	};

	const m::event::fetch event
	{
		std::nothrow, event_idx, fopts
	};

	if(!event.valid)
		return false;

	const auto state_idx
	{
		get(std::nothrow, at<"type"_>(event), at<"state_key"_>(event))
	};

	assert(event_idx);
	return event_idx == state_idx;
}

bool
ircd::m::room::state::has(const string_view &type)
const
{
	if(!present())
	{
		const history history
		{
			room_id, event_id
		};

		return history.has(type);
	}

	return !for_each(type, []
	(const string_view &, const string_view &, const event::idx &)
	{
		return false;
	});
}

bool
ircd::m::room::state::has(const string_view &type,
                          const string_view &state_key)
const
{
	if(!present())
	{
		const history history
		{
			room_id, event_id
		};

		return history.has(type, state_key);
	}

	auto &column{dbs::room_state};
	char key[dbs::ROOM_STATE_KEY_MAX_SIZE];
	return db::has(column, dbs::room_state_key(key, room_id, type, state_key));
}

size_t
ircd::m::room::state::count()
const
{
	if(!present())
	{
		const history history
		{
			room_id, event_id
		};

		return history.count(string_view{});
	}

	const db::gopts &opts
	{
		this->fopts?
			this->fopts->gopts:
			db::gopts{}
	};

	size_t ret(0);
	auto &column{dbs::room_state};
	for(auto it{column.begin(room_id, opts)}; bool(it); ++it)
		++ret;

	return ret;
}

size_t
ircd::m::room::state::count(const string_view &type)
const
{
	if(!present())
	{
		const history history
		{
			room_id, event_id
		};

		return history.count(type);
	}

	size_t ret(0);
	for_each(type, [&ret]
	(const string_view &, const string_view &, const event::idx &)
	{
		++ret;
		return true;
	});

	return ret;
}

void
ircd::m::room::state::for_each(const event::closure &closure)
const
{
	for_each(event::closure_bool{[&closure]
	(const m::event &event)
	{
		closure(event);
		return true;
	}});
}

bool
ircd::m::room::state::for_each(const event::closure_bool &closure)
const
{
	event::fetch event
	{
		fopts? *fopts : event::fetch::default_opts
	};

	return for_each(event::closure_idx_bool{[&event, &closure]
	(const event::idx &event_idx)
	{
		if(seek(std::nothrow, event, event_idx))
			if(!closure(event))
				return false;

		return true;
	}});
}

void
ircd::m::room::state::for_each(const event::id::closure &closure)
const
{
	for_each(event::id::closure_bool{[&closure]
	(const event::id &event_id)
	{
		closure(event_id);
		return true;
	}});
}

bool
ircd::m::room::state::for_each(const event::id::closure_bool &closure)
const
{
	return for_each(event::closure_idx_bool{[&closure]
	(const event::idx &idx)
	{
		bool ret{true};
		m::event_id(std::nothrow, idx, [&ret, &closure]
		(const event::id &id)
		{
			ret = closure(id);
		});

		return ret;
	}});
}

void
ircd::m::room::state::for_each(const event::closure_idx &closure)
const
{
	for_each(event::closure_idx_bool{[&closure]
	(const event::idx &event_idx)
	{
		closure(event_idx);
		return true;
	}});
}

bool
ircd::m::room::state::for_each(const event::closure_idx_bool &closure)
const
{
	return for_each(closure_bool{[&closure]
	(const string_view &type, const string_view &state_key, const event::idx &event_idx)
	{
		return closure(event_idx);
	}});
}

bool
ircd::m::room::state::for_each(const closure_bool &closure)
const
{
	if(!present())
	{
		const history history
		{
			room_id, event_id
		};

		return history.for_each([&closure]
		(const auto &type, const auto &state_key, const auto &depth, const auto &event_idx)
		{
			return closure(type, state_key, event_idx);
		});
	}

	db::gopts opts
	{
		this->fopts? this->fopts->gopts : db::gopts{}
	};

	if(!opts.readahead)
		opts.readahead = size_t(readahead_size);

	auto &column{dbs::room_state};
	for(auto it{column.begin(room_id, opts)}; bool(it); ++it)
	{
		const byte_view<event::idx> idx(it->second);
		const auto key(dbs::room_state_key(it->first));
		if(!closure(std::get<0>(key), std::get<1>(key), idx))
			return false;
	}

	return true;
}

bool
ircd::m::room::state::for_each(const type_prefix &prefix,
                               const closure_bool &closure)
const
{
	bool ret(true), cont(true);
	for_each(closure_bool{[&prefix, &closure, &ret, &cont]
	(const string_view &type, const string_view &state_key, const event::idx &event_idx)
	{
		if(!startswith(type, string_view(prefix)))
			return cont;

		cont = false;
		ret = closure(type, state_key, event_idx);
		return ret;
	}});

	return ret;
}

void
ircd::m::room::state::for_each(const string_view &type,
                               const event::closure &closure)
const
{
	for_each(type, event::closure_bool{[&closure]
	(const m::event &event)
	{
		closure(event);
		return true;
	}});
}

bool
ircd::m::room::state::for_each(const string_view &type,
                               const event::closure_bool &closure)
const
{
	return type?
		for_each(type, string_view{}, closure):
		for_each(closure);
}

void
ircd::m::room::state::for_each(const string_view &type,
                               const event::id::closure &closure)
const
{
	for_each(type, event::id::closure_bool{[&closure]
	(const event::id &event_id)
	{
		closure(event_id);
		return true;
	}});
}

bool
ircd::m::room::state::for_each(const string_view &type,
                               const event::id::closure_bool &closure)
const
{
	return type?
		for_each(type, string_view{}, closure):
		for_each(closure);
}

void
ircd::m::room::state::for_each(const string_view &type,
                               const event::closure_idx &closure)
const
{
	for_each(type, event::closure_idx_bool{[&closure]
	(const event::idx &event_idx)
	{
		closure(event_idx);
		return true;
	}});
}

bool
ircd::m::room::state::for_each(const string_view &type,
                               const event::closure_idx_bool &closure)
const
{
	return type?
		for_each(type, string_view{}, closure):
		for_each(closure);
}

bool
ircd::m::room::state::for_each(const string_view &type,
                               const closure_bool &closure)
const
{
	return type?
		for_each(type, string_view{}, closure):
		for_each(closure);
}

bool
ircd::m::room::state::for_each(const string_view &type,
                               const string_view &state_key_lb,
                               const event::closure_bool &closure)
const
{
	event::fetch event
	{
		fopts? *fopts : event::fetch::default_opts
	};

	return for_each(type, state_key_lb, event::closure_idx_bool{[&event, &closure]
	(const event::idx &event_idx)
	{
		if(seek(std::nothrow, event, event_idx))
			if(!closure(event))
				return false;

		return true;
	}});
}

bool
ircd::m::room::state::for_each(const string_view &type,
                               const string_view &state_key_lb,
                               const event::id::closure_bool &closure)
const
{
	return for_each(type, state_key_lb, event::closure_idx_bool{[&closure]
	(const event::idx &idx)
	{
		bool ret{true};
		m::event_id(std::nothrow, idx, [&ret, &closure]
		(const event::id &id)
		{
			ret = closure(id);
		});

		return ret;
	}});
}

bool
ircd::m::room::state::for_each(const string_view &type,
                               const string_view &state_key_lb,
                               const event::closure_idx_bool &closure)
const
{
	return for_each(type, state_key_lb, closure_bool{[&closure]
	(const string_view &type, const string_view &state_key, const event::idx &event_idx)
	{
		return closure(event_idx);
	}});
}

bool
ircd::m::room::state::for_each(const string_view &type,
                               const string_view &state_key_lb,
                               const closure_bool &closure)
const
{
	if(!present())
	{
		const history history
		{
			room_id, event_id
		};

		return history.for_each(type, state_key_lb, [&closure]
		(const auto &type, const auto &state_key, const auto &depth, const auto &event_idx)
		{
			return closure(type, state_key, event_idx);
		});
	}

	char keybuf[dbs::ROOM_STATE_KEY_MAX_SIZE];
	const auto &key
	{
		dbs::room_state_key(keybuf, room_id, type, state_key_lb)
	};

	db::gopts opts
	{
		this->fopts? this->fopts->gopts : db::gopts{}
	};

	if(!opts.readahead)
		opts.readahead = size_t(readahead_size);

	auto &column{dbs::room_state};
	for(auto it{column.begin(key, opts)}; bool(it); ++it)
	{
		const auto key
		{
			dbs::room_state_key(it->first)
		};

		if(std::get<0>(key) != type)
			break;

		const byte_view<event::idx> idx(it->second);
		if(!closure(std::get<0>(key), std::get<1>(key), idx))
			return false;
	}

	return true;
}

/// Figure out if this instance of room::state is presenting the current
/// "present" state of the room or the state of the room at some previous
/// event. This is an important distinction because the present state of
/// the room should provide optimal performance for the functions of this
/// interface by using the present state table. Prior states will use the
/// state btree.
bool
ircd::m::room::state::present()
const
{
	// When no event_id is passed to the state constructor that immediately
	// indicates the present state of the room is sought.
	if(!event_id)
		return true;

	// When the global configuration disables history, always consider the
	// present state. (disabling may yield unexpected incorrect results by
	// returning the present state without error).
	if(!enable_history)
		return true;

	// Check the cached value from a previous false result of this function
	// before doing any real work/IO below. If this function ever returned
	// false it will never return true after.
	if(_not_present)
		return false;

	const auto head_id
	{
		m::head(std::nothrow, room_id)
	};

	// If the event_id passed is exactly the latest event we can obviously
	// consider this the present state.
	if(!head_id || head_id == event_id)
		return true;

	// This result is cacheable because once it's no longer the present
	// it will never be again. Panta chorei kai ouden menei. Note that this
	// is a const member function; the cache variable is an appropriate case
	// for the 'mutable' keyword.
	_not_present = true;
	return false;
}

bool
ircd::m::room::state::is(const event::idx &event_idx)
{
	bool ret{false};
	m::get(event_idx, "state_key", [&ret]
	(const string_view &state_key)
	{
		ret = true;
	});

	return ret;
}

bool
ircd::m::room::state::is(std::nothrow_t,
                         const event::idx &event_idx)
{
	bool ret{false};
	m::get(std::nothrow, event_idx, "state_key", [&ret]
	(const string_view &state_key)
	{
		ret = true;
	});

	return ret;
}

size_t
ircd::m::room::state::purge_replaced(const room::id &room_id)
{
	db::txn txn
	{
		*m::dbs::events
	};

	size_t ret(0);
	m::room::events it
	{
		room_id, uint64_t(0)
	};

	if(!it)
		return ret;

	for(; it; ++it)
	{
		const m::event::idx &event_idx(it.event_idx());
		if(!m::get(std::nothrow, event_idx, "state_key", [](const auto &) {}))
			continue;

		if(!m::event::refs(event_idx).count(m::dbs::ref::NEXT_STATE))
			continue;

		// TODO: erase event
	}

	return ret;
}

bool
ircd::m::room::state::present(const event::idx &event_idx)
{
	static const event::fetch::opts fopts
	{
		event::keys::include { "room_id", "type", "state_key" },
	};

	const m::event::fetch event
	{
		event_idx, fopts
	};

	const m::room room
	{
		at<"room_id"_>(event)
	};

	const m::room::state state
	{
		room
	};

	const auto state_idx
	{
		state.get(std::nothrow, at<"type"_>(event), at<"state_key"_>(event))
	};

	assert(event_idx);
	return state_idx == event_idx;
}

ircd::m::event::idx
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
ircd::m::room::state::next(const event::idx &event_idx,
                           const event::closure_idx_bool &closure)
{
	const m::event::refs refs
	{
		event_idx
	};

	return refs.for_each(dbs::ref::NEXT_STATE, [&closure]
	(const event::idx &event_idx, const dbs::ref &ref)
	{
		assert(ref == dbs::ref::NEXT_STATE);
		return closure(event_idx);
	});
}

bool
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

//
// state::rebuild
//

ircd::m::room::state::rebuild::rebuild(const room::id &room_id)
{
	const m::event::id::buf event_id
	{
		m::head(room_id)
	};

	const m::room::state::history history
	{
		room_id, event_id
	};

	const m::room::state present_state
	{
		room_id
	};

	const bool check_auth
	{
		!m::internal(room_id)
	};

	m::dbs::write_opts opts;
	opts.appendix.reset();
	opts.appendix.set(dbs::appendix::ROOM_STATE);
	opts.appendix.set(dbs::appendix::ROOM_JOINED);
	db::txn txn
	{
		*m::dbs::events
	};

	m::event::fetch event;
	ssize_t added(0), deleted(0);
	present_state.for_each([&opts, &txn, &deleted, &event]
	(const auto &type, const auto &state_key, const auto &event_idx)
	{
		if(!seek(std::nothrow, event, event_idx))
			return true;

		auto _opts(opts);
		_opts.op = db::op::DELETE;
		_opts.event_idx = event_idx;
		dbs::write(txn, event, _opts);
		++deleted;
		return true;
	});

	history.for_each([&opts, &txn, &added, &room_id, &check_auth, &event]
	(const auto &type, const auto &state_key, const auto &depth, const auto &event_idx)
	{
		if(!seek(std::nothrow, event, event_idx))
			return true;

		const auto &[pass, fail]
		{
			check_auth?
				auth::check_present(event):
				room::auth::passfail{true, {}}
		};

		if(likely(pass))
		{
			auto _opts(opts);
			_opts.op = db::op::SET;
			_opts.event_idx = event_idx;
			dbs::write(txn, event, _opts);
			++added;
		}

		if(unlikely(!pass))
			log::logf
			{
				log, log::level::DWARNING,
				"%s in %s present state :%s",
				string_view{event.event_id},
				string_view{room_id},
				what(fail),
			};

		return true;
	});

	log::info
	{
		log, "Present state of %s @ %s rebuild complete with %zu size:%s del:%zd add:%zd (%zd)",
		string_view{room_id},
		string_view{event_id},
		txn.size(),
		pretty(iec(txn.bytes())),
		deleted,
		added,
		(added - deleted),
	};

	txn();
}
