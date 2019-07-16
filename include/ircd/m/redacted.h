// Matrix Construct
//
// Copyright (C) Matrix Construct Developers, Authors & Contributors
// Copyright (C) 2016-2019 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

#pragma once
#define HAVE_IRCD_M_REDACTED_H

namespace ircd::m
{
	struct redacted;
}

struct ircd::m::redacted
{
	bool ret;

  public:
	operator const bool &() const;

	redacted(const event::idx &);
	redacted(const event::id &);
	explicit redacted(const event &);
};

inline
ircd::m::redacted::redacted(const event &event)
:redacted
{
    event.event_id
}
{}

inline
ircd::m::redacted::redacted(const event::id &event_id)
:redacted
{
    index(event_id, std::nothrow)
}
{}

inline ircd::m::redacted::operator
const bool &()
const
{
	return ret;
}
