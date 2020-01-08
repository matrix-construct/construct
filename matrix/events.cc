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
IRCD_MODULE_EXPORT
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
IRCD_MODULE_EXPORT
ircd::m::events::dump__file(const string_view &filename)
{
	const fs::fd file
	{
		filename, std::ios::out | std::ios::app
	};

	// POSIX_FADV_DONTNEED
	fs::evict(file);

	const unique_buffer<mutable_buffer> buf
	{
		size_t(dump_buffer_size)
	};

	char *pos{data(buf)};
	size_t foff{0}, ecount{0}, acount{0}, errcount{0};
	for(auto it(begin(dbs::event_json)); it; ++it) try
	{
		const event::idx seq
		{
			byte_view<uint64_t>(it->first)
		};

		const string_view source
		{
			it->second
		};

		const auto remain
		{
			size_t(data(buf) + size(buf) - pos)
		};

		assert(remain >= event::MAX_SIZE && remain <= size(buf));
		const mutable_buffer mb
		{
			pos, remain
		};

		pos += copy(mb, source);
		++ecount;
		if(pos + event::MAX_SIZE > data(buf) + size(buf))
		{
			const const_buffer cb
			{
				data(buf), pos
			};

			foff += size(fs::append(file, cb));
			pos = data(buf);
			if(acount++ % 256 == 0)
			{
				char pbuf[48];
				log::info
				{
					"dump[%s] %0.2lf%% @ seq %zu of %zu; %zu events; %s in %zu writes; %zu errors",
					filename,
					(seq / double(m::vm::sequence::retired)) * 100.0,
					seq,
					m::vm::sequence::retired,
					ecount,
					pretty(pbuf, iec(foff)),
					acount,
					errcount
				};
			}
		}
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
			"dump[%s] events; %s in %zu writes; %zu errors :%s",
			filename,
			ecount,
			acount,
			errcount,
			e.what(),
		};
	}

	if(pos > data(buf))
	{
		const const_buffer cb{data(buf), pos};
		foff += size(fs::append(file, cb));
		++acount;
	}

	log::notice
	{
		log, "dump[%s] complete events:%zu using %s in writes:%zu errors:%zu",
		filename,
		ecount,
		pretty(iec(foff)),
		acount,
		errcount,
	};
}

bool
IRCD_MODULE_EXPORT
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

		return --limit;
	});
}

bool
IRCD_MODULE_EXPORT
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
			range.first:
			std::min(range.first, vm::sequence::retired)
	};

	const auto stop
	{
		ascending?
			std::min(range.second, vm::sequence::retired + 1):
			range.second
	};

	for(; start != stop; ascending? ++start : --start)
		if(seek(event, start, std::nothrow))
			if(!closure(start, event))
				return false;

	return true;
}

///TODO: This impl is temp. Need better dispatching based on filter before
///TODO: fetching event.
bool
IRCD_MODULE_EXPORT
ircd::m::events::for_each(const range &range,
                          const event_filter &filter,
                          const event::closure_idx_bool &closure)
{
	auto limit
	{
		json::get<"limit"_>(filter)?: 32L
	};

	return for_each(range, event::closure_idx_bool{[&filter, &closure, &limit, &range]
	(const event::idx &event_idx)
	-> bool
	{
		const m::event::fetch event
		{
			event_idx, std::nothrow, range.fopts? *range.fopts : event::fetch::default_opts
		};

		if(!event.valid)
			return true;

		if(!match(filter, event))
			return true;

		if(!closure(event_idx))
			return false;

		return --limit;
	}});

	return true;
}

bool
IRCD_MODULE_EXPORT
ircd::m::events::for_each(const range &range,
                          const event::closure_idx_bool &closure)
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

	auto &column
	{
		dbs::event_json
	};

	auto it
	{
		column.lower_bound(byte_view<string_view>(start))
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

		if(!closure(event_idx))
			return false;
	}

	return true;
}

//
// events::type
//

bool
IRCD_MODULE_EXPORT
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
IRCD_MODULE_EXPORT
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
IRCD_MODULE_EXPORT
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
IRCD_MODULE_EXPORT
ircd::m::events::type::for_each(const string_view &prefix,
                                const closure_name &closure)
{
	db::column &column
	{
		dbs::event_type
	};

	const auto &prefixer
	{
		dbs::desc::events__event_type__pfx
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
IRCD_MODULE_EXPORT
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
IRCD_MODULE_EXPORT
ircd::m::events::origin::for_each(const string_view &prefix,
                                  const closure_name &closure)
{
	db::column &column
	{
		dbs::event_sender
	};

	const auto &prefixer
	{
		dbs::desc::events__event_sender__pfx
	};

	if(unlikely(startswith(prefix, '@')))
		throw panic
		{
			"Prefix argument should be a hostname. It must not start with '@'"
		};

	string_view last;
	char buf[rfc3986::DOMAIN_BUFSIZE];
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
IRCD_MODULE_EXPORT
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
IRCD_MODULE_EXPORT
ircd::m::events::sender::for_each(const string_view &prefix_,
                                  const closure_name &closure)
{
	db::column &column
	{
		dbs::event_sender
	};

	const auto &prefixer
	{
		dbs::desc::events__event_sender__pfx
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
