// Matrix Construct
//
// Copyright (C) Matrix Construct Developers, Authors & Contributors
// Copyright (C) 2016-2023 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

#pragma once
#define HAVE_IRCD_M_REPLACED_H

namespace ircd::m
{
	struct replaced;
}

class ircd::m::replaced
:public returns<event::idx>
{
	m::relates relates;

  public:
	IRCD_OVERLOAD(latest);

	replaced(const event::idx &, latest_t);
	replaced(const event::idx &);

	replaced(const event::id &, latest_t);
	replaced(const event::id &);

	explicit replaced(const event &, latest_t);
	explicit replaced(const event &);
};

inline
ircd::m::replaced::replaced(const event &event)
:replaced{event.event_id}
{}

inline
ircd::m::replaced::replaced(const event &event,
                            latest_t)
:replaced{event.event_id, latest}
{}

inline
ircd::m::replaced::replaced(const event::id &event_id)
:replaced{index(std::nothrow, event_id)}
{}

inline
ircd::m::replaced::replaced(const event::id &event_id,
                            latest_t)
:replaced{index(std::nothrow, event_id), latest}
{}

inline
ircd::m::replaced::replaced(const event::idx &event_idx)
:relates
{
	.refs = event_idx,
	.match_sender = true,
}
{
	this->returns::ret = relates.has("m.replace")? -1UL: 0UL;
}

inline
ircd::m::replaced::replaced(const event::idx &event_idx,
                            latest_t)
:relates
{
	.refs = event_idx,
	.match_sender = true,
}
{
	this->returns::ret = relates.latest("m.replace");
}
