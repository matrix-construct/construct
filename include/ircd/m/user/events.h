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
#define HAVE_IRCD_M_USER_EVENTS_H

/// Interface to all events from a user (sender)
struct ircd::m::user::events
{
	using idx_closure_bool = std::function<bool (const event::idx &)>;
	using closure_bool = std::function<bool (const event &)>;

	m::user user;

  public:
	bool for_each(const idx_closure_bool &) const;
	bool for_each(const closure_bool &) const;
	size_t count() const;

	events(const m::user &user);
};
