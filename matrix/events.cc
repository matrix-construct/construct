// Matrix Construct
//
// Copyright (C) Matrix Construct Developers, Authors & Contributors
// Copyright (C) 2016-2019 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

namespace ircd::m::events
{
	extern conf::item<size_t> dump_buffer_size;
}

decltype(ircd::m::events::dump_buffer_size)
ircd::m::events::dump_buffer_size
{
	{ "name",     "ircd.m.events.dump.buffer_size" },
	{ "default",  int64_t(512_KiB)                 },
};

void
ircd::m::events::rebuild()
{
	static const event::fetch::opts fopts
	{
		event::keys::include {"type", "sender"}
	};

	static const m::events::range range
	{
		0, -1UL, &fopts
	};

	db::txn txn
	{
		*m::dbs::events
	};

	dbs::write_opts wopts;
	wopts.appendix.reset();
	wopts.appendix.set(dbs::appendix::EVENT_TYPE);
	wopts.appendix.set(dbs::appendix::EVENT_SENDER);

	size_t ret(0);
	for_each(range, [&txn, &wopts, &ret]
	(const event::idx &event_idx, const m::event &event)
	{
		wopts.event_idx = event_idx;
		dbs::write(txn, event, wopts);
		++ret;

		if(ret % 8192UL == 0UL)
			log::info
			{
				log, "Events type/sender table rebuild events %zu of %zu num:%zu txn:%zu %s",
				event_idx,
				vm::sequence::retired,
				ret,
				txn.size(),
				pretty(iec(txn.bytes())),
			};

		return true;
	});

	log::info
	{
		log, "Events type/sender table rebuild events:%zu txn:%zu %s commit...",
		ret,
		txn.size(),
		pretty(iec(txn.bytes())),
	};

	txn();

	log::notice
	{
		log, "Events type/sender table rebuild complete.",
		ret,
		txn.size(),
		pretty(iec(txn.bytes())),
	};
}

void
ircd::m::events::dump__file(const string_view &filename)
{
	static const db::gopts gopts
	{
		db::get::NO_CACHE, db::get::NO_CHECKSUM
	};

	static const fs::fd::opts fileopts
	{
		std::ios::out
	};

	auto _fileopts(fileopts);
	_fileopts.exclusive = true;   // error if exists
	_fileopts.dontneed = true;    // fadvise
	const fs::fd file
	{
		filename, _fileopts
	};

	const unique_buffer<mutable_buffer> buf
	{
		size_t(dump_buffer_size), 512
	};

	util::timer timer;
	event::idx seq{0};
	size_t foff{0}, ecount{0}, acount{0}, errcount{0};
	const auto flusher{[&](const const_buffer &buf)
	{
		const const_buffer wrote
		{
			fs::append(file, buf)
		};

		foff += size(wrote);
		if(acount++ % 256 == 0)
		{
			const auto elapsed
			{
				std::max(timer.at<seconds>().count(), 1L)
			};

			char pbuf[3][48];
			log::info
			{
				"dump[%s] %0.2lf%% @ seq %zu of %zu; %zu events; %zu events/s; wrote %s; %s/s; %s elapsed; errors %zu",
				filename,
				(seq / double(m::vm::sequence::retired)) * 100.0,
				seq,
				m::vm::sequence::retired,
				ecount,
				(ecount / elapsed),
				pretty(pbuf[0], iec(foff)),
				pretty(pbuf[1], iec(foff / elapsed), 1),
				ircd::pretty(pbuf[2], seconds(elapsed)),
				errcount,
			};
		}

		return wrote;
	}};

	json::stack out
	{
		buf,
		flusher,
		-1UL,               // high watermark
		size(buf) - 64_KiB  // low watermark
	};

	json::stack::array top
	{
		out
	};

	for(auto it(dbs::event_json.begin(gopts)); it; ++it) try
	{
		seq = byte_view<uint64_t>
		{
			it->first
		};

		const json::object source
		{
			it->second
		};

		const json::stack::checkpoint cp
		{
			out
		};

		top.append(source);
		++ecount;
	}
	catch(const ctx::interrupted &)
	{
		throw;
	}
	catch(const std::exception &e)
	{
		++errcount;
		log::error
		{
			"dump[%s] %zu events; %zu writes; %zu errors :%s",
			filename,
			ecount,
			acount,
			errcount,
			e.what(),
		};
	}

	top.~array();
	out.flush(true);

	char pbuf[48];
	log::notice
	{
		log, "dump[%s] complete events:%zu using %s in writes:%zu errors:%zu; %s elapsed",
		filename,
		ecount,
		pretty(iec(foff)),
		acount,
		errcount,
		timer.pretty(pbuf),
	};
}

bool
ircd::m::events::for_each(const range &range,
                          const event_filter &filter,
                          const closure &closure)
{
	auto limit
	{
		json::get<"limit"_>(filter)?: 32L
	};

	return for_each(range, [&filter, &closure, &limit]
	(const event::idx &event_idx, const m::event &event)
	-> bool
	{
		if(!match(filter, event))
			return true;

		if(!closure(event_idx, event))
			return false;

		return --limit > 0L;
	});
}

bool
ircd::m::events::for_each(const range &range,
                          const closure &closure)
{
	event::fetch event
	{
		range.fopts? *range.fopts : event::fetch::default_opts
	};

	const bool ascending
	{
		range.first <= range.second
	};

	auto start
	{
		ascending?
			std::min(range.first, vm::sequence::retired + 1):
			std::min(range.first, vm::sequence::retired)
	};

	const auto stop
	{
		ascending?
			std::min(range.second, vm::sequence::retired + 1):
			range.second
	};

	for(; start != stop; ascending? ++start : --start)
		if(seek(std::nothrow, event, start))
			if(!closure(start, event))
				return false;

	return true;
}

//
// events::source
//

namespace ircd::m::events::source
{
	extern conf::item<size_t> readahead;
}

decltype(ircd::m::events::source::readahead)
ircd::m::events::source::readahead
{
	{ "name",     "ircd.m.events.source.readahead" },
	{ "default",  long(4_MiB)                      },
};

bool
ircd::m::events::source::for_each(const range &range,
                                  const closure &closure)
{
	const bool ascending
	{
		range.first <= range.second
	};

	auto start
	{
		ascending?
			range.first:
			std::min(range.first, vm::sequence::retired)
	};

	const auto stop
	{
		ascending?
			std::min(range.second, vm::sequence::retired + 1):
			range.second
	};

	db::gopts gopts
	{
		db::get::NO_CACHE,
		db::get::NO_CHECKSUM
	};
	gopts.readahead = size_t(readahead);
	gopts.readahead &= boolmask<size_t>(ascending);

	auto it
	{
		dbs::event_json.lower_bound(byte_view<string_view>(start), gopts)
	};

	for(; bool(it); ascending? ++it : --it)
	{
		const event::idx event_idx
		{
			byte_view<event::idx>(it->first)
		};

		if(ascending && event_idx >= stop)
			break;

		if(!ascending && event_idx <= stop)
			break;

		const json::object &event
		{
			it->second
		};

		if(!closure(event_idx, event))
			return false;
	}

	return true;
}

//
// events::content
//

namespace ircd::m::events::content
{
	extern conf::item<size_t> readahead;
}

decltype(ircd::m::events::content::readahead)
ircd::m::events::content::readahead
{
	{ "name",     "ircd.m.events.content.readahead" },
	{ "default",  long(4_MiB)                       },
};

bool
ircd::m::events::content::for_each(const closure &closure)
{
	constexpr auto content_idx
	{
		json::indexof<event, "content"_>()
	};

	db::column &column
	{
		dbs::event_column.at(content_idx)
	};

	db::gopts gopts
	{
		db::get::NO_CACHE,
		db::get::NO_CHECKSUM
	};
	gopts.readahead = size_t(readahead);

	auto it(column.begin(gopts));
	for(; it; ++it)
	{
		const auto &event_idx
		{
			byte_view<uint64_t>(it->first)
		};

		const json::object &content
		{
			it->second
		};

		if(!closure(event_idx, content))
			return false;
	}

	return true;
}

//
// events::refs
//

namespace ircd::m::events::refs
{
	extern conf::item<size_t> readahead;
}

decltype(ircd::m::events::refs::readahead)
ircd::m::events::refs::readahead
{
	{ "name",     "ircd.m.events.refs.readahead" },
	{ "default",  long(512_KiB)                  },
};

bool
ircd::m::events::refs::for_each(const range &range,
                                const closure &closure)
{
	db::column &event_refs
	{
		dbs::event_refs
	};

    db::gopts gopts
    {
        db::get::NO_CACHE,
        db::get::NO_CHECKSUM
    };
	gopts.readahead = size_t(readahead);

	const auto start
	{
		std::min(range.first, range.second)
	};

	const auto stop
	{
		std::max(range.first, range.second)
	};

	auto it
	{
		event_refs.lower_bound(byte_view<string_view>(start), gopts)
	};

	for(; it; ++it)
	{
		const auto &key
		{
			it->first
		};

		const event::idx src
		{
			byte_view<event::idx, false>(key)
		};

		if(src >= stop)
			break;

		const auto &[type, tgt]
		{
			dbs::event_refs_key(key.substr(sizeof(event::idx)))
		};

		assert(tgt != src);
		if(!closure(src, type, tgt))
			return false;
	}

	return true;
}

//
// events::state
//

bool
ircd::m::events::state::for_each(const closure &closure)
{
	static const tuple none
	{
		{}, {}, {}, -1L, 0UL
	};

	return state::for_each(none, [&closure]
	(const tuple &key) -> bool
	{
		return closure(key);
	});
}

bool
ircd::m::events::state::for_each(const tuple &query,
                                 const closure &closure)
{
	db::column &column
	{
		dbs::event_state
	};

	char buf[dbs::EVENT_STATE_KEY_MAX_SIZE];
	const string_view query_key
	{
		dbs::event_state_key(buf, query)
	};

	for(auto it(column.lower_bound(query_key)); bool(it); ++it)
	{
		const auto key
		{
			dbs::event_state_key(it->first)
		};

		const auto &[state_key, type, room_id, depth, event_idx]
		{
			key
		};

		const bool tab[]
		{
			!std::get<0>(query) || std::get<0>(query) == std::get<0>(key),
			!std::get<1>(query) || std::get<1>(query) == std::get<1>(key),
			!std::get<2>(query) || std::get<2>(query) == std::get<2>(key),
			std::get<3>(query) <= 0 || std::get<3>(query) == std::get<3>(key),
			std::get<4>(query) == 0 || std::get<4>(query) == std::get<4>(key),
		};

		if(!std::all_of(begin(tab), end(tab), identity()))
			break;

		if(!closure(key))
			return false;
	}

	return true;
}

//
// events::type
//

bool
ircd::m::events::type::has(const string_view &type)
{
	bool ret{false};
	for_each(type, [&ret, &type]
	(const string_view &type_)
	{
		ret = type == type_;
		return false; // uncond break out of loop after first result
	});

	return ret;
}

bool
ircd::m::events::type::has_prefix(const string_view &type)
{
	bool ret{false};
	for_each(type, [&ret, &type]
	(const string_view &type_)
	{
		ret = startswith(type_, type);
		return false; // uncond break out of loop after first result
	});

	return ret;
}

bool
ircd::m::events::type::for_each_in(const string_view &type,
                                   const closure &closure)
{
	auto &column
	{
		dbs::event_type
	};

	char buf[dbs::EVENT_TYPE_KEY_MAX_SIZE];
	const string_view &key
	{
		dbs::event_type_key(buf, type)
	};

	auto it
	{
		column.begin(key)
	};

	for(; bool(it); ++it)
	{
		const auto &keyp
		{
			dbs::event_type_key(it->first)
		};

		if(!closure(type, std::get<0>(keyp)))
			return false;
	}

	return true;
}

bool
ircd::m::events::type::for_each(const string_view &prefix,
                                const closure_name &closure)
{
	db::column &column
	{
		dbs::event_type
	};

	const auto &prefixer
	{
		dbs::desc::event_type__pfx
	};

	string_view last;
	char lastbuf[event::TYPE_MAX_SIZE];
	for(auto it(column.lower_bound(prefix)); bool(it); ++it)
	{
		const auto &type
		{
			prefixer.get(it->first)
		};

		if(type == last)
			continue;

		if(prefix && !startswith(type, prefix))
			break;

		last =
		{
			lastbuf, copy(lastbuf, type)
		};

		if(!closure(type))
			return false;
	}

	return true;
}

//
// events::origin
//

bool
ircd::m::events::origin::for_each_in(const string_view &origin,
                                     const sender::closure &closure)
{
	auto &column
	{
		dbs::event_sender
	};

	char buf[dbs::EVENT_SENDER_KEY_MAX_SIZE];
	const string_view &key
	{
		dbs::event_sender_origin_key(buf, origin)
	};

	auto it
	{
		column.begin(key)
	};

	for(; bool(it); ++it)
	{
		const auto &keyp
		{
			dbs::event_sender_origin_key(it->first)
		};

		const user::id::buf user_id
		{
			std::get<0>(keyp), origin
		};

		if(!closure(user_id, std::get<1>(keyp)))
			return false;
	}

	return true;
}

bool
ircd::m::events::origin::for_each(const string_view &prefix,
                                  const closure_name &closure)
{
	db::column &column
	{
		dbs::event_sender
	};

	const auto &prefixer
	{
		dbs::desc::event_sender__pfx
	};

	if(unlikely(startswith(prefix, '@')))
		throw panic
		{
			"Prefix argument should be a hostname. It must not start with '@'"
		};

	string_view last;
	char buf[event::ORIGIN_MAX_SIZE];
	for(auto it(column.lower_bound(prefix)); bool(it); ++it)
	{
		if(!m::dbs::is_event_sender_origin_key(it->first))
			break;

		const string_view &host
		{
			prefixer.get(it->first)
		};

		if(host == last)
			continue;

		if(!startswith(host, prefix))
			break;

		last = { buf, copy(buf, host) };
		if(!closure(host))
			return false;
	}

	return true;
}

//
// events::sender
//

bool
ircd::m::events::sender::for_each_in(const id::user &user,
                                     const closure &closure)
{
	auto &column
	{
		dbs::event_sender
	};

	char buf[dbs::EVENT_SENDER_KEY_MAX_SIZE];
	const string_view &key
	{
		dbs::event_sender_key(buf, user)
	};

	auto it
	{
		column.begin(key)
	};

	for(; bool(it); ++it)
	{
		const auto &keyp
		{
			dbs::event_sender_key(it->first)
		};

		if(!closure(user, std::get<event::idx>(keyp)))
			return false;
	}

	return true;
}

bool
ircd::m::events::sender::for_each(const string_view &prefix_,
                                  const closure_name &closure)
{
	db::column &column
	{
		dbs::event_sender
	};

	const auto &prefixer
	{
		dbs::desc::event_sender__pfx
	};

	// We MUST query the column with a key starting with '@' here. For a more
	// convenient API, if the user did not supply an '@' we prefix it for them.
	char prebuf[id::user::buf::SIZE] {"@"};
	const string_view prefix
	{
		!startswith(prefix_, '@')?
			strlcat{prebuf, prefix_}:
			prefix_
	};

	m::user::id::buf last;
	for(auto it(column.lower_bound(prefix)); bool(it); ++it)
	{
		// Check if this is an '@' key; otherwise it's in the origin
		// keyspace (sharing this column) which we don't want here.
		if(!m::dbs::is_event_sender_key(it->first))
			break;

		// Apply the domain prefixer, since we're iterating as a db::column
		// rather than db::domain.
		const m::user::id &user_id
		{
			prefixer.get(it->first)
		};

		if(user_id == last)
			continue;

		if(!startswith(user_id, prefix))
			break;

		if(!closure(user_id))
			return false;

		last = user_id;
	}

	return true;
}
