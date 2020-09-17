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
ircd::m::index(std::nothrow_t,
               const event &event)
{
	return index(std::nothrow, event.event_id);
}

ircd::m::event::idx
ircd::m::index(const event::id &event_id)
{
	assert(event_id);
	const auto ret
	{
		index(std::nothrow, event_id)
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
ircd::m::index(std::nothrow_t,
               const event::id &event_id)
{
	auto &column
	{
		dbs::event_idx
	};

	bool found;
	alignas(8) char buf[8] {0};
	if(likely(event_id))
		read(column, event_id, found, buf);

	constexpr bool safety(false); // we know buf is the right size
	return byte_view<event::idx, safety>
	{
		buf
	};
}

bool
ircd::m::index(std::nothrow_t,
               const event::id &event_id,
               const event::closure_idx &closure)
{
	auto &column
	{
		dbs::event_idx
	};

	return event_id && column(event_id, std::nothrow, [&closure]
	(const string_view &value)
	{
		const event::idx event_idx
		{
			byte_view<event::idx>(value)
		};

		closure(event_idx);
	});
}
