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
			"%s for %s not found in database",
			key,
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
			"%s for event_idx[%lu] not found in database",
			key,
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
			"%s for %s not found in database",
			key,
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
			key,
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

	const auto &column_idx
	{
		json::indexof<event>(key)
	};

	auto &column
	{
		dbs::event_column.at(column_idx)
	};

	const string_view &column_key
	{
		byte_view<string_view>{event_idx}
	};

	if(column)
		return column(column_key, std::nothrow, closure);

	// If the event property being sought doesn't have its own column we
	// fall back to fetching the full JSON and closing over the property.
	bool ret{false};
	dbs::event_json(column_key, std::nothrow, [&closure, &key, &ret]
	(const json::object &event)
	{
		string_view value
		{
			event[key]
		};

		if(!value)
			return;

		// The user expects an unquoted string to their closure, the same as
		// if this value was found in its own column.
		if(json::type(value, json::STRING))
			value = json::string(value);

		ret = true;
		closure(value);
	});

	return ret;
}
