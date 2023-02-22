// Matrix Construct
//
// Copyright (C) Matrix Construct Developers, Authors & Contributors
// Copyright (C) 2016-2018 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

///////////////////////////////////////////////////////////////////////////////
//
// event/get.h
//

std::string
ircd::m::get(const event::id &event_id,
             const string_view &key)
{
	std::string ret;
	get(event_id, key, [&ret]
	(const string_view &value)
	{
		ret = value;
	});

	return ret;
}

std::string
ircd::m::get(const event::idx &event_idx,
             const string_view &key)
{
	std::string ret;
	get(event_idx, key, [&ret]
	(const string_view &value)
	{
		ret = value;
	});

	return ret;
}

std::string
ircd::m::get(std::nothrow_t,
             const event::id &event_id,
             const string_view &key)
{
	std::string ret;
	get(std::nothrow, event_id, key, [&ret]
	(const string_view &value)
	{
		ret = value;
	});

	return ret;
}

std::string
ircd::m::get(std::nothrow_t,
             const event::idx &event_idx,
             const string_view &key)
{
	std::string ret;
	get(std::nothrow, event_idx, key, [&ret]
	(const string_view &value)
	{
		ret = value;
	});

	return ret;
}

ircd::const_buffer
ircd::m::get(const event::id &event_id,
             const string_view &key,
             const mutable_buffer &out)
{
	const auto &ret
	{
		get(std::nothrow, index(event_id), key, out)
	};

	if(!ret)
		throw m::NOT_FOUND
		{
			"%s for %s not found in database.",
			key?: "<_event_json>",
			string_view{event_id}
		};

	return ret;
}

ircd::const_buffer
ircd::m::get(const event::idx &event_idx,
             const string_view &key,
             const mutable_buffer &out)
{
	const const_buffer ret
	{
		get(std::nothrow, event_idx, key, out)
	};

	if(!ret)
		throw m::NOT_FOUND
		{
			"%s for event_idx[%lu] not found in database.",
			key?: "<_event_json>",
			event_idx
		};

	return ret;
}

ircd::const_buffer
ircd::m::get(std::nothrow_t,
             const event::id &event_id,
             const string_view &key,
             const mutable_buffer &buf)
{
	return get(std::nothrow, index(std::nothrow, event_id), key, buf);
}

ircd::const_buffer
ircd::m::get(std::nothrow_t,
             const event::idx &event_idx,
             const string_view &key,
             const mutable_buffer &buf)
{
	const_buffer ret;
	get(std::nothrow, event_idx, key, [&buf, &ret]
	(const string_view &value)
	{
		ret = { data(buf), copy(buf, value) };
	});

	return ret;
}

void
ircd::m::get(const event::id &event_id,
             const string_view &key,
             const event::fetch::view_closure &closure)
{
	if(!get(std::nothrow, index(event_id), key, closure))
		throw m::NOT_FOUND
		{
			"%s for %s not found in database.",
			key?: "<_event_json>",
			string_view{event_id}
		};
}

void
ircd::m::get(const event::idx &event_idx,
             const string_view &key,
             const event::fetch::view_closure &closure)
{
	if(!get(std::nothrow, event_idx, key, closure))
		throw m::NOT_FOUND
		{
			"%s for event_idx[%lu] not found in database",
			key?: "<_event_json>",
			event_idx
		};
}

bool
ircd::m::get(std::nothrow_t,
             const event::id &event_id,
             const string_view &key,
             const event::fetch::view_closure &closure)
{
	return get(std::nothrow, index(std::nothrow, event_id), key, closure);
}

bool
ircd::m::get(std::nothrow_t,
             const event::idx &event_idx,
             const string_view &key,
             const event::fetch::view_closure &closure)
{
	if(!event_idx)
		return false;

	const string_view &column_key
	{
		byte_view<string_view>{event_idx}
	};

	const auto &column_idx
	{
		json::indexof<event>(key)
	};

	if(column_idx < dbs::event_column.size())
	{
		auto &column
		{
			dbs::event_column.at(column_idx)
		};

		if(column)
			return column(column_key, std::nothrow, closure);
	}

	// If the event property being sought doesn't have its own column we
	// fall back to fetching the full JSON and closing over the property.
	bool ret{false};
	dbs::event_json(column_key, std::nothrow, [&closure, &key, &ret]
	(const json::object &event)
	{
		string_view value
		{
			key?
				event[key]:
				string_view{event}
		};

		if(!value)
			return;

		// The user expects an unquoted string to their closure, the same as
		// if this value was found in its own column.
		if(key && json::type(value, json::STRING))
			value = json::string(value);

		ret = true;
		closure(value);
	});

	return ret;
}

void
ircd::m::get(const vector_view<const event::idx> &event_idx,
             const string_view &key,
             const event::fetch::views_closure &closure)
{
	const auto mask
	{
		get(std::nothrow, event_idx, key, closure)
	};

	const auto found
	{
		popcount(mask)
	};

	if(unlikely(size_t(found) < event_idx.size()))
		throw m::NOT_FOUND
		{
			"Only %zu/%zu for %s found in database",
			found,
			event_idx.size(),
			key?: "<_event_json>",
		};
}

uint64_t
ircd::m::get(std::nothrow_t,
             const vector_view<const event::idx> &event_idx,
             const string_view &key,
             const event::fetch::views_closure &closure)
{
	const auto &column_idx
	{
		json::indexof<event>(key)
	};

	auto &column
	{
		dbs::event_column.at(column_idx)
	};

	if(unlikely(!column))
		throw panic
		{
			"Parallel fetch not yet supported for key '%s'",
			key,
		};

	if(!event_idx.size())
		return 0;

	static const auto MAX {64UL};
	const auto &num
	{
		std::min(event_idx.size(), MAX)
	};

	string_view column_key[num];
	for(uint i(0); i < num; ++i)
		column_key[i] = byte_view<string_view>
		{
			event_idx[i]
		};

	const vector_view<const string_view> keys
	{
		column_key, num
	};

	return column(keys, std::nothrow, [&closure]
	(const vector_view<const string_view> &res)
	{
		closure(res);
	});
}
