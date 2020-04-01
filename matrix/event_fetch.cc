// Matrix Construct
//
// Copyright (C) Matrix Construct Developers, Authors & Contributors
// Copyright (C) 2016-2018 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

//
// seek
//

void
ircd::m::seek(event::fetch &fetch,
              const event::id &event_id)
{
	if(!seek(fetch, event_id, std::nothrow))
		throw m::NOT_FOUND
		{
			"%s not found in database", event_id
		};
}

bool
ircd::m::seek(event::fetch &fetch,
              const event::id &event_id,
              std::nothrow_t)
{
	const auto &event_idx
	{
		index(event_id, std::nothrow)
	};

	return seek(fetch, event_idx, event_id, std::nothrow);
}

void
ircd::m::seek(event::fetch &fetch,
              const event::idx &event_idx)
{
	if(!seek(fetch, event_idx, std::nothrow))
		throw m::NOT_FOUND
		{
			"%lu not found in database", event_idx
		};
}

bool
ircd::m::seek(event::fetch &fetch,
              const event::idx &event_idx,
              std::nothrow_t)
{
	return seek(fetch, event_idx, m::event::id{}, std::nothrow);
}

bool
ircd::m::seek(event::fetch &fetch,
              const event::idx &event_idx,
              const event::id &event_id,
              std::nothrow_t)
{
	fetch.event_idx = event_idx;
	fetch.event_id_buf = event_id?
		event::id::buf{event_id}:
		event::id::buf{};

	if(!event_idx)
	{
		fetch.valid = false;
		return fetch.valid;
	}

	const string_view &key
	{
		byte_view<string_view>(event_idx)
	};

	auto &event
	{
		static_cast<m::event &>(fetch)
	};

	assert(fetch.fopts);
	const auto &opts(*fetch.fopts);
	if(!fetch.should_seek_json(opts))
		if((fetch.valid = db::seek(fetch.row, key, opts.gopts)))
			if((fetch.valid = fetch.assign_from_row(key)))
				return fetch.valid;

	if((fetch.valid = fetch._json.load(key, opts.gopts)))
		fetch.valid = fetch.assign_from_json(key);

	return fetch.valid;
}

//
// event::fetch
//

decltype(ircd::m::event::fetch::default_opts)
ircd::m::event::fetch::default_opts
{};

//
// event::fetch::fetch
//

/// Seek to event_id and populate this event from database.
/// Throws if event not in database.
ircd::m::event::fetch::fetch(const event::id &event_id,
                             const opts &opts)
:fetch
{
	index(event_id), event_id, std::nothrow, opts
}
{
	if(!valid)
		throw m::NOT_FOUND
		{
			"%s not found in database", string_view{event_id}
		};
}

/// Seek to event_id and populate this event from database.
/// Event is not populated if not found in database.
ircd::m::event::fetch::fetch(const event::id &event_id,
                             std::nothrow_t,
                             const opts &opts)
:fetch
{
	index(event_id, std::nothrow), event_id, std::nothrow, opts
}
{
}

/// Seek to event_idx and populate this event from database.
/// Throws if event not in database.
ircd::m::event::fetch::fetch(const event::idx &event_idx,
                             const opts &opts)
:fetch
{
	event_idx, std::nothrow, opts
}
{
	if(!valid)
		throw m::NOT_FOUND
		{
			"idx %zu not found in database", event_idx
		};
}

ircd::m::event::fetch::fetch(const event::idx &event_idx,
                             std::nothrow_t,
                             const opts &opts)
:fetch
{
	event_idx, m::event::id{}, std::nothrow, opts
}
{
}

/// Seek to event_idx and populate this event from database.
/// Event is not populated if not found in database.
ircd::m::event::fetch::fetch(const event::idx &event_idx,
                             const event::id &event_id,
                             std::nothrow_t,
                             const opts &opts)
:fopts
{
	&opts
}
,event_idx
{
	event_idx
}
,_json
{
	dbs::event_json,
	event_idx && should_seek_json(opts)?
		key(&event_idx):
		string_view{},
	opts.gopts
}
,row
{
	*dbs::events,
	event_idx && !_json.valid(key(&event_idx))?
		key(&event_idx):
		string_view{},
	event_idx && !_json.valid(key(&event_idx))?
		event::keys{opts.keys}:
		event::keys{event::keys::include{}},
	cell,
	opts.gopts
}
,valid
{
	false
}
,event_id_buf
{
	event_id?
		event::id::buf{event_id}:
		event::id::buf{}
}
{
	valid =
		event_idx && _json.valid(key(&event_idx))?
			assign_from_json(key(&event_idx)):
		event_idx?
			assign_from_row(key(&event_idx)):
			false;
}

/// Seekless constructor.
ircd::m::event::fetch::fetch(const opts &opts)
:fopts
{
	&opts
}
,_json
{
	dbs::event_json,
	string_view{},
	opts.gopts
}
,row
{
	*dbs::events,
	string_view{},
	!should_seek_json(opts)?
		event::keys{opts.keys}:
		event::keys{event::keys::include{}},
	cell,
	opts.gopts
}
,valid
{
	false
}
{
}

bool
ircd::m::event::fetch::assign_from_json(const string_view &key)
try
{
	auto &event
	{
		static_cast<m::event &>(*this)
	};

	assert(_json.valid(key));
	const json::object source
	{
		_json.val()
	};

	assert(!empty(source));
	const bool source_event_id
	{
		!event_id_buf && source.has("event_id")
	};

	const auto event_id
	{
		source_event_id?
			id(json::string(source.at("event_id"))):
		event_id_buf?
			id(event_id_buf):
			m::event_id(std::nothrow, event_idx, event_id_buf)
	};

	assert(fopts);
	assert(event_id);
	event =
	{
		source, event_id, event::keys{fopts->keys}
	};

	assert(data(event.source) == data(source));
	assert(event.event_id == event_id);
	return true;
}
catch(const json::parse_error &e)
{
	const ctx::exception_handler eh;

	const auto event_id
	{
		event_id_buf?
			id(event_id_buf):
			m::event_id(std::nothrow, event_idx, event_id_buf)
	};

	log::critical
	{
		m::log, "Fetching event:%lu %s JSON from local database :%s",
		event_idx,
		string_view{event_id},
		e.what(),
	};

	return false;
}

bool
ircd::m::event::fetch::assign_from_row(const string_view &key)
try
{
	auto &event
	{
		static_cast<m::event &>(*this)
	};

	if(!row.valid(key))
		return false;

	event.source = {};
	assign(event, row, key);
	const auto event_id
	{
		!empty(json::get<"event_id"_>(event))?
			id{json::get<"event_id"_>(event)}:
		event_id_buf?
			id{event_id_buf}:
			m::event_id(std::nothrow, event_idx, event_id_buf)
	};

	assert(event_id);
	event.event_id = event_id;
	return true;
}
catch(const json::parse_error &e)
{
	const ctx::exception_handler eh;

	const auto event_id
	{
		event_id_buf?
			id(event_id_buf):
			m::event_id(std::nothrow, event_idx, event_id_buf)
	};

	log::critical
	{
		m::log, "Fetching event:%lu %s JSON from local database :%s",
		event_idx,
		string_view{event_id},
		e.what(),
	};

	return false;
}

bool
ircd::m::event::fetch::should_seek_json(const opts &opts)
{
	// User always wants to make the event_json query regardless
	// of their keys selection.
	if(opts.query_json_force)
		return true;

	// If and only if selected keys have direct columns we can return
	// false to seek direct columns. If any other keys are selected we
	/// must perform the event_json query instead.
	for(size_t i(0); i < opts.keys.size(); ++i)
		if(opts.keys.test(i))
			if(!dbs::event_column.at(i))
				return true;

	return false;
}

ircd::string_view
ircd::m::event::fetch::key(const event::idx *const &event_idx)
{
	assert(event_idx);
	return byte_view<string_view>(*event_idx);
}

//
// event::fetch::opts
//

ircd::m::event::fetch::opts::opts(const db::gopts &gopts,
                                  const event::keys::selection &keys)
:opts
{
	keys, gopts
}
{
}

ircd::m::event::fetch::opts::opts(const event::keys::selection &keys,
                                  const db::gopts &gopts)
:keys{keys}
,gopts{gopts}
{
}

ircd::m::event::fetch::opts::opts()
noexcept
{
}
