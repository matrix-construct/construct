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

ircd::mapi::header
IRCD_MODULE
{
	"Matrix events library"
};

decltype(ircd::m::events::dump_buffer_size)
ircd::m::events::dump_buffer_size
{
	{ "name",     "ircd.m.events.dump.buffer_size" },
	{ "default",  int64_t(4_MiB)                   },
};

void
IRCD_MODULE_EXPORT
ircd::m::events::dump__file(const string_view &filename)
{
	const fs::fd file
	{
		filename, std::ios::out | std::ios::app
	};

	const unique_buffer<mutable_buffer> buf
	{
		size_t(dump_buffer_size)
	};

	char *pos{data(buf)};
	size_t foff{0}, ecount{0}, acount{0}, errcount{0};
	for_each(m::event::idx{0}, [&]
	(const event::idx &seq, const m::event &event)
	{
		const auto remain
		{
			size_t(data(buf) + size(buf) - pos)
		};

		assert(remain >= event::MAX_SIZE && remain <= size(buf));
		const mutable_buffer mb{pos, remain};
		pos += copy(mb, event.source);
		++ecount;

		if(pos + event::MAX_SIZE > data(buf) + size(buf))
		{
			const const_buffer cb{data(buf), pos};
			foff += size(fs::append(file, cb));
			pos = data(buf);
			++acount;

			const double pct
			{
				(seq / double(m::vm::sequence::retired)) * 100.0
			};

			log::info
			{
				"dump[%s] %0.2lf$%c @ seq %zu of %zu; %zu events; %zu bytes; %zu writes; %zu errors",
				filename,
				pct,
				'%', //TODO: fix gram
				seq,
				m::vm::sequence::retired,
				ecount,
				foff,
				acount,
				errcount
			};
		}

		return true;
	});

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
                          const closure_bool &closure)
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
                          const closure_bool &closure)
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

bool
IRCD_MODULE_EXPORT
ircd::m::events::for_each_in_origin(const string_view &origin,
                                    const closure_sender_bool &closure)
{
	auto &column
	{
		dbs::event_sender
	};

	char buf[dbs::EVENT_SENDER_KEY_MAX_SIZE];
	const string_view &key
	{
		dbs::event_sender_key(buf, origin)
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

		char _buf[m::id::MAX_SIZE];
		mutable_buffer buf{_buf};
		consume(buf, copy(buf, std::get<0>(keyp)));
		consume(buf, copy(buf, ":"_sv));
		consume(buf, copy(buf, origin));
		const string_view &user_id
		{
			_buf, data(buf)
		};

		assert(valid(id::USER, user_id));
		if(!closure(user_id, std::get<1>(keyp)))
			return false;
	}

	return true;
}

bool
IRCD_MODULE_EXPORT
ircd::m::events::for_each_in_sender(const id::user &user,
                                    const closure_sender_bool &closure)
{
	auto &column
	{
		dbs::event_sender
	};

	char buf[dbs::EVENT_SENDER_KEY_MAX_SIZE];
	const string_view &key
	{
		dbs::event_sender_key(buf, user, 0)
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

		if(std::get<0>(keyp) != user.local())
			break;

		if(!closure(user, std::get<1>(keyp)))
			return false;
	}

	return true;
}

bool
IRCD_MODULE_EXPORT
ircd::m::events::for_each_in_type(const string_view &type,
                                  const closure_type_bool &closure)
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
ircd::m::events::for_each_type(const closure_type_name_bool &closure)
{
	return for_each_type(string_view{}, closure);
}

bool
IRCD_MODULE_EXPORT
ircd::m::events::for_each_type(const string_view &prefix,
                               const closure_type_name_bool &closure)
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

		if(!startswith(type, prefix))
			break;

		last = { lastbuf, copy(lastbuf, type) };
		if(!closure(type))
			return false;
	}

	return true;
}

bool
IRCD_MODULE_EXPORT
ircd::m::events::for_each_sender(const closure_sender_name_bool &closure)
{
	return for_each_sender(string_view{}, closure);
}

bool
IRCD_MODULE_EXPORT
ircd::m::events::for_each_sender(const string_view &hostlb,
                                 const closure_sender_name_bool &closure)
{
	db::column &column
	{
		dbs::event_sender
	};

	const auto &prefixer
	{
		dbs::desc::events__event_sender__pfx
	};

	user::id::buf last;
	for(auto it(column.lower_bound(hostlb)); bool(it); ++it)
	{
		const string_view &host
		{
			prefixer.get(it->first)
		};

		if(!startswith(host, hostlb))
			break;

		const auto &[localpart, event_idx]
		{
			dbs::event_sender_key(it->first.substr(size(host)))
		};

		if(last && host == last.host() && localpart == last.local())
			continue;

		last = m::user::id::buf
		{
			localpart, host
		};

		if(!closure(last))
			return false;
	}

	return true;
}

bool
IRCD_MODULE_EXPORT
ircd::m::events::for_each_origin(const closure_origin_name_bool &closure)
{
	return for_each_origin(string_view{}, closure);
}

bool
IRCD_MODULE_EXPORT
ircd::m::events::for_each_origin(const string_view &prefix,
                                 const closure_origin_name_bool &closure)
{
	db::column &column
	{
		dbs::event_sender
	};

	const auto &prefixer
	{
		dbs::desc::events__event_sender__pfx
	};

	string_view last;
	char buf[rfc3986::DOMAIN_BUFSIZE];
	for(auto it(column.lower_bound(prefix)); bool(it); ++it)
	{
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
