// Matrix Construct
//
// Copyright (C) Matrix Construct Developers, Authors & Contributors
// Copyright (C) 2016-2018 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

ircd::m::event::id::buf
ircd::m::event_id(const event::idx &event_idx)
{
	event::id::buf ret;
	event_id(event_idx, ret);
	return ret;
}

ircd::m::event::id::buf
ircd::m::event_id(std::nothrow_t,
                  const event::idx &event_idx)
{
	event::id::buf ret;
	event_id(std::nothrow, event_idx, ret);
	return ret;
}

ircd::m::event::id
ircd::m::event_id(const event::idx &event_idx,
                  event::id::buf &buf)
{
	const event::id ret
	{
		event_id(std::nothrow, event_idx, buf)
	};

	if(!ret)
		throw m::NOT_FOUND
		{
			"Cannot find event ID from idx[%lu]",
			event_idx
		};

	return ret;
}

ircd::m::event::id
ircd::m::event_id(std::nothrow_t,
                  const event::idx &event_idx,
                  event::id::buf &buf)
{
	event_id(std::nothrow, event_idx, [&buf]
	(const event::id &eid)
	{
		buf = eid;
	});

	return buf?
		event::id{buf}:
		event::id{};
}

bool
ircd::m::event_id(std::nothrow_t,
                  const event::idx &event_idx,
                  const event::id::closure &closure)
{
	return get(std::nothrow, event_idx, "event_id", closure);
}
