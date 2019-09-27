// Matrix Construct
//
// Copyright (C) Matrix Construct Developers, Authors & Contributors
// Copyright (C) 2016-2018 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

ircd::m::event::idx
ircd::m::index(const event &event)
try
{
	return index(event.event_id);
}
catch(const json::not_found &)
{
	throw m::NOT_FOUND
	{
		"Cannot find index for event without an event_id."
	};
}

ircd::m::event::idx
ircd::m::index(const event &event,
               std::nothrow_t)
{
	return index(event.event_id, std::nothrow);
}

ircd::m::event::idx
ircd::m::index(const event::id &event_id)
{
	assert(event_id);
	const auto ret
	{
		index(event_id, std::nothrow)
	};

	if(!ret)
		throw m::NOT_FOUND
		{
			"no index found for %s",
			string_view{event_id}

		};

	return ret;
}

ircd::m::event::idx
ircd::m::index(const event::id &event_id,
               std::nothrow_t)
{
	event::idx ret{0};
	index(event_id, std::nothrow, [&ret]
	(const event::idx &event_idx)
	{
		ret = event_idx;
	});

	return ret;
}

bool
ircd::m::index(const event::id &event_id,
               std::nothrow_t,
               const event::closure_idx &closure)
{
	auto &column
	{
		dbs::event_idx
	};

	if(!event_id)
		return false;

	return column(event_id, std::nothrow, [&closure]
	(const string_view &value)
	{
		const event::idx &event_idx
		{
			byte_view<event::idx>(value)
		};

		closure(event_idx);
	});
}
