// The Construct
//
// Copyright (C) The Construct Developers, Authors & Contributors
// Copyright (C) 2016-2020 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

namespace ircd::m::dbs
{
	static size_t _prefetch_event_refs_m_room_redaction(const event &, const opts &);
	static void _index_event_refs_m_room_redaction(db::txn &, const event &, const opts &); //query
	static size_t _prefetch_event_refs_m_receipt_m_read(const event &, const opts &);
	static void _index_event_refs_m_receipt_m_read(db::txn &, const event &, const opts &); //query
	static size_t _prefetch_event_refs_m_relates_m_reply(const event &, const opts &);
	static void _index_event_refs_m_relates_m_reply(db::txn &, const event &, const opts &); //query
	static size_t _prefetch_event_refs_m_relates(const event &, const opts &);
	static void _index_event_refs_m_relates(db::txn &, const event &, const opts &); //query
	static size_t _prefetch_event_refs_state(const event &, const opts &);
	static void _index_event_refs_state(db::txn &, const event &, const opts &); // query
	static size_t _prefetch_event_refs_auth(const event &, const opts &);
	static void _index_event_refs_auth(db::txn &, const event &, const opts &); //query
	static size_t _prefetch_event_refs_prev(const event &, const opts &);
	static void _index_event_refs_prev(db::txn &, const event &, const opts &); //query
	static size_t _prefetch_delete_horizon(const event &, const opts &);
	static void _index_delete_horizon(db::txn &, const event &, const opts &); // query
	static bool event_refs__cmp_less(const string_view &a, const string_view &b);
}

decltype(ircd::m::dbs::event_refs)
ircd::m::dbs::event_refs;

decltype(ircd::m::dbs::desc::event_refs__comp)
ircd::m::dbs::desc::event_refs__comp
{
	{ "name",     "ircd.m.dbs._event_refs.comp" },
	{ "default",  "default"                     },
};

decltype(ircd::m::dbs::desc::event_refs__block__size)
ircd::m::dbs::desc::event_refs__block__size
{
	{ "name",     "ircd.m.dbs._event_refs.block.size" },
	{ "default",  512L                                },
};

decltype(ircd::m::dbs::desc::event_refs__meta_block__size)
ircd::m::dbs::desc::event_refs__meta_block__size
{
	{ "name",     "ircd.m.dbs._event_refs.meta_block.size" },
	{ "default",  512L                                     },
};

decltype(ircd::m::dbs::desc::event_refs__cache__size)
ircd::m::dbs::desc::event_refs__cache__size
{
	{
		{ "name",     "ircd.m.dbs._event_refs.cache.size" },
		{ "default",  long(32_MiB)                        },
	},
	[](conf::item<void> &)
	{
		const size_t &value{event_refs__cache__size};
		db::capacity(db::cache(dbs::event_refs), value);
	}
};

decltype(ircd::m::dbs::desc::event_refs__cache_comp__size)
ircd::m::dbs::desc::event_refs__cache_comp__size
{
	{
		{ "name",     "ircd.m.dbs._event_refs.cache_comp.size" },
		{ "default",  long(0_MiB)                              },
	},
	[](conf::item<void> &)
	{
		const size_t &value{event_refs__cache_comp__size};
		db::capacity(db::cache_compressed(dbs::event_refs), value);
	}
};

const ircd::db::prefix_transform
ircd::m::dbs::desc::event_refs__pfx
{
	"_event_refs",
	[](const string_view &key) noexcept
	{
		return size(key) >= sizeof(event::idx) * 2;
	},

	[](const string_view &key) noexcept
	{
		assert(size(key) >= sizeof(event::idx));
		return string_view
		{
			data(key), data(key) + sizeof(event::idx)
		};
	}
};

const ircd::db::comparator
ircd::m::dbs::desc::event_refs__cmp
{
	"_event_refs",
	event_refs__cmp_less,
	db::cmp_string_view::equal,
};

const ircd::db::descriptor
ircd::m::dbs::desc::event_refs
{
	.name = "_event_refs",
	.explain = R"(Inverse reference graph of events.

	event_idx | ref, event_idx => --

	The first part of the key is the event being referenced. The second part
	of the key is the event which refers to the first event somewhere in its
	prev_events references. The event_idx in the second part of the key also
	contains a dbs::ref type in its highest order byte so we can store
	different kinds of references.

	The prefix transform is in effect; an event may be referenced multiple
	times. We can find all the events we have which reference a target, and
	why. The database must already contain both events (hence they have
	event::idx numbers).

	The value is currently unused/empty; we may eventually store metadata with
	information about this reference (i.e. is depth adjacent? is the ref
	redundant with another in the same event and should not be made? etc).

	)",

	.type =
	{
		typeid(uint64_t), typeid(string_view)
	},

	.cmp = event_refs__cmp,
	.prefix = event_refs__pfx,
	.cache_size = bool(cache_enable)? -1 : 0, //uses conf item
	.cache_size_comp = bool(cache_comp_enable)? -1 : 0,
	.bloom_bits = 0,
	.expect_queries_hit = true,
	.block_size = size_t(event_refs__block__size),
	.meta_block_size = size_t(event_refs__meta_block__size),
	.compression = string_view{event_refs__comp},
	.compaction_pri = "kOldestSmallestSeqFirst"s,
};

//
// indexers
//

void
ircd::m::dbs::_index_event_refs(db::txn &txn,
                                const event &event,
                                const opts &opts)
{
	assert(opts.appendix.test(appendix::EVENT_REFS));

	if(opts.op == db::op::DELETE)
		if(opts.appendix.test(appendix::EVENT_HORIZON))
			_index_delete_horizon(txn, event, opts);

	if(opts.event_refs.test(uint(ref::NEXT)))
		_index_event_refs_prev(txn, event, opts);

	if(opts.event_refs.test(uint(ref::NEXT_AUTH)))
		_index_event_refs_auth(txn, event, opts);

	if(opts.event_refs.test(uint(ref::NEXT_STATE)) ||
	   opts.event_refs.test(uint(ref::PREV_STATE)))
		_index_event_refs_state(txn, event, opts);

	if(opts.event_refs.test(uint(ref::M_RECEIPT__M_READ)))
		_index_event_refs_m_receipt_m_read(txn, event, opts);

	if(opts.event_refs.test(uint(ref::M_RELATES)))
		_index_event_refs_m_relates(txn, event, opts);

	if(opts.event_refs.test(uint(ref::M_RELATES)))
		_index_event_refs_m_relates_m_reply(txn, event, opts);

	if(opts.event_refs.test(uint(ref::M_ROOM_REDACTION)))
		_index_event_refs_m_room_redaction(txn, event, opts);
}

size_t
ircd::m::dbs::_prefetch_event_refs(const event &event,
                                   const opts &opts)
{
	assert(opts.appendix.test(appendix::EVENT_REFS));

	size_t ret(0);
	if(opts.op == db::op::DELETE)
		if(opts.appendix.test(appendix::EVENT_HORIZON))
			ret += _prefetch_delete_horizon(event, opts);

	if(opts.event_refs.test(uint(ref::NEXT)))
		ret += _prefetch_event_refs_prev(event, opts);

	if(opts.event_refs.test(uint(ref::NEXT_AUTH)))
		ret += _prefetch_event_refs_auth(event, opts);

	if(opts.event_refs.test(uint(ref::NEXT_STATE)) ||
	   opts.event_refs.test(uint(ref::PREV_STATE)))
		ret += _prefetch_event_refs_state(event, opts);

	if(opts.event_refs.test(uint(ref::M_RECEIPT__M_READ)))
		ret += _prefetch_event_refs_m_receipt_m_read(event, opts);

	if(opts.event_refs.test(uint(ref::M_RELATES)))
		ret += _prefetch_event_refs_m_relates(event, opts);

	if(opts.event_refs.test(uint(ref::M_RELATES)))
		ret += _prefetch_event_refs_m_relates_m_reply(event, opts);

	if(opts.event_refs.test(uint(ref::M_ROOM_REDACTION)))
		ret += _prefetch_event_refs_m_room_redaction(event, opts);

	return ret;
}

// NOTE: QUERY
void
ircd::m::dbs::_index_delete_horizon(db::txn &txn,
                                    const event &event,
                                    const opts &opts)
{
	assert(opts.event_idx);
	assert(opts.op == db::op::DELETE);
	assert(opts.appendix.test(appendix::EVENT_HORIZON));

	const event::refs refs
	{
		opts.event_idx
	};

	refs.for_each([&txn, &event, &opts]
	(const auto &event_idx, const auto &type)
	{
		if(!opts.event_refs.test(uint(type)))
			return;

		auto _opts(opts);
		_opts.op = db::op::SET;
		_opts.event_idx = event_idx;
		_index_event_horizon(txn, {}, _opts, event.event_id);
	});
}

size_t
ircd::m::dbs::_prefetch_delete_horizon(const event &event,
                                       const opts &opts)
{
	assert(opts.event_idx);
	assert(opts.op == db::op::DELETE);
	assert(opts.appendix.test(appendix::EVENT_HORIZON));

	const event::refs refs
	{
		opts.event_idx
	};

	return refs.prefetch();
}

// NOTE: QUERY
void
ircd::m::dbs::_index_event_refs_prev(db::txn &txn,
                                     const event &event,
                                     const opts &opts)
{
	assert(opts.appendix.test(appendix::EVENT_REFS));
	assert(opts.event_refs.test(uint(ref::NEXT)));

	const event::prev prev
	{
		event
	};

	event::id prev_ids[prev.MAX];
	const auto &prev_id
	{
		prev.ids(prev_ids)
	};

	event::idx prev_idx[prev.MAX] {0};
	const auto &found
	{
		find_event_idx(prev_idx, prev_id, opts)
	};

	for(size_t i(0); i < prev_id.size(); ++i)
	{
		if(opts.appendix.test(appendix::EVENT_HORIZON))
			if(!prev_idx[i])
			{
				_index_event_horizon(txn, event, opts, prev_id[i]);
				continue;
			}

		if(!prev_idx[i])
		{
			log::dwarning
			{
				log, "No index found to ref %s PREV of %s",
				string_view{prev_id[i]},
				string_view{event.event_id},
			};

			continue;
		}

		assert(opts.event_idx != prev_idx[i]);
		assert(prev_idx[i] != 0 && opts.event_idx != 0);
		thread_local char buf[EVENT_REFS_KEY_MAX_SIZE];
		const string_view &key
		{
			event_refs_key(buf, prev_idx[i], ref::NEXT, opts.event_idx)
		};

		db::txn::append
		{
			txn, dbs::event_refs,
			{
				opts.op, key
			}
		};
	}
}

size_t
ircd::m::dbs::_prefetch_event_refs_prev(const event &event,
                                        const opts &opts)
{
	assert(opts.appendix.test(appendix::EVENT_REFS));
	assert(opts.event_refs.test(uint(ref::NEXT)));

	const event::prev prev
	{
		event
	};

	event::id prev_ids[prev.MAX];
	const vector_view<const event::id> &prev_id
	(
		prev.ids(prev_ids)
	);

	return prefetch_event_idx(prev_id, opts);
}

// NOTE: QUERY
void
ircd::m::dbs::_index_event_refs_auth(db::txn &txn,
                                     const event &event,
                                     const opts &opts)
{
	assert(opts.appendix.test(appendix::EVENT_REFS));
	assert(opts.event_refs.test(uint(ref::NEXT_AUTH)));

	if(!m::room::auth::is_power_event(event))
		return;

	const event::auth auth
	{
		event
	};

	event::id auth_ids[auth.MAX];
	const auto &auth_id
	{
		auth.ids(auth_ids)
	};

	event::idx auth_idx[auth.MAX] {0};
	const auto &found
	{
		find_event_idx(auth_idx, auth_id, opts)
	};

	for(size_t i(0); i < auth_id.size(); ++i)
	{
		if(opts.appendix.test(appendix::EVENT_HORIZON))
			if(unlikely(!auth_idx[i]))
				_index_event_horizon(txn, event, opts, auth_id[i]);

		if(unlikely(!auth_idx[i]))
		{
			log::error
			{
				log, "No index found to ref %s AUTH of %s",
				string_view{auth_id[i]},
				string_view{event.event_id}
			};

			continue;
		}

		thread_local char buf[EVENT_REFS_KEY_MAX_SIZE];
		assert(opts.event_idx != 0 && auth_idx[i] != 0);
		assert(opts.event_idx != auth_idx[i]);
		const string_view &key
		{
			event_refs_key(buf, auth_idx[i], ref::NEXT_AUTH, opts.event_idx)
		};

		db::txn::append
		{
			txn, dbs::event_refs,
			{
				opts.op, key
			}
		};
	}
}

size_t
ircd::m::dbs::_prefetch_event_refs_auth(const event &event,
                                        const opts &opts)
{
	assert(opts.appendix.test(appendix::EVENT_REFS));
	assert(opts.event_refs.test(uint(ref::NEXT_AUTH)));

	if(!m::room::auth::is_power_event(event))
		return false;

	const event::auth auth
	{
		event
	};

	event::id auth_ids[auth.MAX];
	const vector_view<const event::id> auth_id
	(
		auth.ids(auth_ids)
	);

	return prefetch_event_idx(auth_id, opts);
}

// NOTE: QUERY
void
ircd::m::dbs::_index_event_refs_state(db::txn &txn,
                                      const event &event,
                                      const opts &opts)
{
	assert(opts.appendix.test(appendix::EVENT_REFS));
	assert(opts.event_refs.test(uint(ref::NEXT_STATE)) ||
	       opts.event_refs.test(uint(ref::PREV_STATE)));

	if(!json::get<"room_id"_>(event))
		return;

	if(!defined(json::get<"state_key"_>(event)))
		return;

	const m::room room
	{
		at<"room_id"_>(event) //TODO: ABA ABA ABA ABA
	};

	const m::room::state state
	{
		room
	};

	const event::idx &prev_state_idx
	{
		opts.allow_queries?
			state.get(std::nothrow, at<"type"_>(event), at<"state_key"_>(event)): // query
			0UL
	};

	// No previous state; nothing to do.
	if(!prev_state_idx)
		return;

	// If the previous state's event_idx is greater than the event_idx of the
	// event we're transacting this is almost surely a replay/rewrite. Bail
	// out for now rather than corrupting the graph.
	if(unlikely(prev_state_idx >= opts.event_idx))
		return;

	thread_local char buf[EVENT_REFS_KEY_MAX_SIZE];
	assert(opts.event_idx != 0 && prev_state_idx != 0);
	assert(opts.event_idx != prev_state_idx);

	if(opts.event_refs.test(uint(ref::NEXT_STATE)))
	{
		const string_view &key
		{
			event_refs_key(buf, prev_state_idx, ref::NEXT_STATE, opts.event_idx)
		};

		db::txn::append
		{
			txn, dbs::event_refs,
			{
				opts.op, key
			}
		};
	}

	if(opts.event_refs.test(uint(ref::PREV_STATE)))
	{
		const string_view &key
		{
			event_refs_key(buf, opts.event_idx, ref::PREV_STATE, prev_state_idx)
		};

		db::txn::append
		{
			txn, dbs::event_refs,
			{
				opts.op, key
			}
		};
	}
}

size_t
ircd::m::dbs::_prefetch_event_refs_state(const event &event,
                                         const opts &opts)
{
	assert(opts.appendix.test(appendix::EVENT_REFS));
	assert(opts.event_refs.test(uint(ref::NEXT_STATE)) ||
	       opts.event_refs.test(uint(ref::PREV_STATE)));

	assert(json::get<"type"_>(event));
	assert(json::get<"room_id"_>(event));
	if(!defined(json::get<"state_key"_>(event)))
		return false;

	const m::room room
	{
		json::get<"room_id"_>(event)
	};

	const m::room::state state
	{
		room
	};

	return state.prefetch(json::get<"type"_>(event), json::get<"state_key"_>(event));
}

// NOTE: QUERY
void
ircd::m::dbs::_index_event_refs_m_receipt_m_read(db::txn &txn,
                                                 const event &event,
                                                 const opts &opts)
{
	assert(opts.appendix.test(appendix::EVENT_REFS));
	assert(opts.event_refs.test(uint(ref::M_RECEIPT__M_READ)));

	if(json::get<"type"_>(event) != "ircd.read")
		return;

	if(!my_host(json::get<"origin"_>(event)))
		return;

	//TODO: disallow local forge?

	const json::string &event_id
	{
		json::get<"content"_>(event).get("event_id")
	};

	if(!valid(m::id::EVENT, event_id))
		return;

	const event::idx &event_idx
	{
		find_event_idx(event_id, opts)
	};

	if(opts.appendix.test(appendix::EVENT_HORIZON))
		if(!event_idx)
		{
			_index_event_horizon(txn, event, opts, event_id);
			return;
		}

	if(!event_idx)
	{
		log::dwarning
		{
			log, "No index found to ref %s M_RECEIPT__M_READ of %s",
			string_view{event_id},
			string_view{event.event_id}
		};

		return;
	}

	thread_local char buf[EVENT_REFS_KEY_MAX_SIZE];
	assert(opts.event_idx != 0 && event_idx != 0);
	assert(opts.event_idx != event_idx);
	const string_view &key
	{
		event_refs_key(buf, event_idx, ref::M_RECEIPT__M_READ, opts.event_idx)
	};

	db::txn::append
	{
		txn, dbs::event_refs,
		{
			opts.op, key
		}
	};
}

size_t
ircd::m::dbs::_prefetch_event_refs_m_receipt_m_read(const event &event,
                                                    const opts &opts)
{
	assert(opts.appendix.test(appendix::EVENT_REFS));
	assert(opts.event_refs.test(uint(ref::M_RECEIPT__M_READ)));

	if(json::get<"type"_>(event) != "ircd.read")
		return false;

	if(!my_host(json::get<"origin"_>(event)))
		return false;

	const json::string &event_id
	{
		json::get<"content"_>(event).get("event_id")
	};

	if(unlikely(!valid(m::id::EVENT, event_id)))
		return false;

	return prefetch_event_idx(event_id, opts);
}

// NOTE: QUERY
void
ircd::m::dbs::_index_event_refs_m_relates(db::txn &txn,
                                          const event &event,
                                          const opts &opts)
{
	assert(opts.appendix.test(appendix::EVENT_REFS));
	assert(opts.event_refs.test(uint(ref::M_RELATES)));

	const auto &content
	{
		json::get<"content"_>(event)
	};

	if(!content.has("m.relates_to", json::OBJECT))
		return;

	const json::object &m_relates_to
	{
		content["m.relates_to"]
	};

	const json::string &event_id
	{
		m_relates_to["event_id"]
	};

	if(!event_id)
		return;

	if(unlikely(!valid(m::id::EVENT, event_id)))
	{
		log::derror
		{
			log, "Cannot index m.relates_to in %s; '%s' is not an event_id.",
			string_view{event.event_id},
			string_view{event_id}
		};

		return;
	}

	const event::idx &event_idx
	{
		find_event_idx(event_id, opts)
	};

	if(opts.appendix.test(appendix::EVENT_HORIZON))
		if(!event_idx)
		{
			_index_event_horizon(txn, event, opts, event_id);
			return;
		}

	if(!event_idx)
	{
		log::derror
		{
			log, "Cannot index m.relates_to in %s; referenced %s not found.",
			string_view{event.event_id},
			string_view{event_id}
		};

		return;
	}

	thread_local char buf[EVENT_REFS_KEY_MAX_SIZE];
	assert(opts.event_idx != 0 && event_idx != 0);
	assert(opts.event_idx != event_idx);
	const string_view &key
	{
		event_refs_key(buf, event_idx, ref::M_RELATES, opts.event_idx)
	};

	db::txn::append
	{
		txn, dbs::event_refs,
		{
			opts.op, key
		}
	};
}

size_t
ircd::m::dbs::_prefetch_event_refs_m_relates(const event &event,
                                             const opts &opts)
{
	assert(opts.appendix.test(appendix::EVENT_REFS));
	assert(opts.event_refs.test(uint(ref::M_RELATES)));

	const auto &content
	{
		json::get<"content"_>(event)
	};

	if(!content.has("m.relates_to", json::OBJECT))
		return false;

	const json::object &m_relates_to
	{
		content["m.relates_to"]
	};

	const json::string &event_id
	{
		m_relates_to["event_id"]
	};

	if(unlikely(!valid(m::id::EVENT, event_id)))
		return false;

	return prefetch_event_idx(event_id, opts);
}

// NOTE: QUERY
void
ircd::m::dbs::_index_event_refs_m_relates_m_reply(db::txn &txn,
                                                  const event &event,
                                                  const opts &opts)
{
	assert(opts.appendix.test(appendix::EVENT_REFS));
	assert(opts.event_refs.test(uint(ref::M_RELATES)));

	if(json::get<"type"_>(event) != "m.room.message")
		return;

	const m::room::message content
	{
		json::get<"content"_>(event)
	};

	const m::event::id event_id
	{
		content.reply_to_event()
	};

	if(!event_id)
		return;

	const event::idx &event_idx
	{
		find_event_idx(event_id, opts)
	};

	if(opts.appendix.test(appendix::EVENT_HORIZON))
		if(!event_idx)
		{
			_index_event_horizon(txn, event, opts, event_id);
			return;
		}

	if(!event_idx)
	{
		log::dwarning
		{
			log, "Cannot index m.in_reply_to in %s; referenced %s not found.",
			string_view{event.event_id},
			string_view{event_id}
		};

		return;
	}

	thread_local char buf[EVENT_REFS_KEY_MAX_SIZE];
	assert(opts.event_idx != 0 && event_idx != 0);
	assert(opts.event_idx != event_idx);
	const string_view &key
	{
		event_refs_key(buf, event_idx, ref::M_RELATES, opts.event_idx)
	};

	db::txn::append
	{
		txn, dbs::event_refs,
		{
			opts.op, key
		}
	};
}

size_t
ircd::m::dbs::_prefetch_event_refs_m_relates_m_reply(const event &event,
                                                     const opts &opts)
{
	assert(opts.appendix.test(appendix::EVENT_REFS));
	assert(opts.event_refs.test(uint(ref::M_RELATES)));

	if(json::get<"type"_>(event) != "m.room.message")
		return false;

	const m::room::message content
	{
		json::get<"content"_>(event)
	};

	const m::event::id event_id
	{
		content.reply_to_event()
	};

	if(!event_id)
		return false;

	return prefetch_event_idx(event_id, opts);
}

// NOTE: QUERY
void
ircd::m::dbs::_index_event_refs_m_room_redaction(db::txn &txn,
                                                 const event &event,
                                                 const opts &opts)
{
	assert(opts.appendix.test(appendix::EVENT_REFS));
	assert(opts.event_refs.test(uint(ref::M_ROOM_REDACTION)));

	if(json::get<"type"_>(event) != "m.room.redaction")
		return;

	if(!valid(m::id::EVENT, json::get<"redacts"_>(event)))
		return;

	const auto &event_id
	{
		json::get<"redacts"_>(event)
	};

	const event::idx &event_idx
	{
		find_event_idx(event_id, opts)
	};

	if(opts.appendix.test(appendix::EVENT_HORIZON))
		if(!event_idx)
		{
			_index_event_horizon(txn, event, opts, event_id);
			return;
		}

	if(!event_idx)
	{
		log::dwarning
		{
			log, "Cannot index m.room.redaction in %s; referenced %s not found.",
			string_view{event.event_id},
			string_view{event_id}
		};

		return;
	}

	thread_local char buf[EVENT_REFS_KEY_MAX_SIZE];
	assert(opts.event_idx != 0 && event_idx != 0);
	assert(opts.event_idx != event_idx);
	const string_view &key
	{
		event_refs_key(buf, event_idx, ref::M_ROOM_REDACTION, opts.event_idx)
	};

	db::txn::append
	{
		txn, dbs::event_refs,
		{
			opts.op, key
		}
	};
}

size_t
ircd::m::dbs::_prefetch_event_refs_m_room_redaction(const event &event,
                                                    const opts &opts)
{
	assert(opts.appendix.test(appendix::EVENT_REFS));
	assert(opts.event_refs.test(uint(ref::M_ROOM_REDACTION)));

	if(json::get<"type"_>(event) != "m.room.redaction")
		return false;

	const auto &event_id
	{
		json::get<"redacts"_>(event)
	};

	if(!valid(m::id::EVENT, event_id))
		return false;

	return prefetch_event_idx(event_id, opts);
}

//
// cmp
//

bool
ircd::m::dbs::event_refs__cmp_less(const string_view &a,
                                   const string_view &b)
{
	static const size_t half(sizeof(event::idx));
	static const size_t full(half * 2);

	assert(size(a) >= half);
	assert(size(b) >= half);
	const event::idx *const key[2]
	{
		reinterpret_cast<const event::idx *>(data(a)),
		reinterpret_cast<const event::idx *>(data(b)),
	};

	return
		key[0][0] < key[1][0]?   true:
		key[0][0] > key[1][0]?   false:
		size(a) < size(b)?       true:
		size(a) > size(b)?       false:
		size(a) == half?         false:
		key[0][1] < key[1][1]?   true:
		                         false;
}

//
// key
//

ircd::m::dbs::event_refs_tuple
ircd::m::dbs::event_refs_key(const string_view &amalgam)
{
	const event::idx key
	{
		byte_view<event::idx>{amalgam}
	};

	return
	{
		ref(key >> ref_shift), key & ~ref_mask
	};
}

ircd::string_view
ircd::m::dbs::event_refs_key(const mutable_buffer &out,
                             const event::idx &tgt,
                             const ref &type,
                             const event::idx &src)
{
	assert((src & ref_mask) == 0);
	assert(size(out) >= sizeof(event::idx) * 2);
	event::idx *const &key
	{
		reinterpret_cast<event::idx *>(data(out))
	};

	key[0] = tgt;
	key[1] = src;
	key[1] |= uint64_t(type) << ref_shift;
	return string_view
	{
		data(out), data(out) + sizeof(event::idx) * 2
	};
}

//
// util
//

ircd::string_view
ircd::m::dbs::reflect(const ref &type)
{
	switch(type)
	{
		case ref::NEXT:                  return "NEXT";
		case ref::NEXT_AUTH:             return "NEXT_AUTH";
		case ref::NEXT_STATE:            return "NEXT_STATE";
		case ref::PREV_STATE:            return "PREV_STATE";
		case ref::M_RECEIPT__M_READ:     return "M_RECEIPT__M_READ";
		case ref::M_RELATES:             return "M_RELATES";
		case ref::M_ROOM_REDACTION:      return "M_ROOM_REDACTION";
	}

	return "????";
}
