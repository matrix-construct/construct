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
#define HAVE_IRCD_M_USER_BREADCRUMB_ROOMS_H

struct ircd::m::user::breadcrumb_rooms
{
	using closure = std::function<void (const string_view &)>;
	using closure_bool = std::function<bool (const string_view &)>;
	using sort_closure = std::function<bool (const string_view &, const string_view &)>;

	m::user::account_data account_data;

  public:
	bool get(std::nothrow_t, const closure &) const;
	void get(const closure &) const;

	bool for_each(const closure_bool &) const;

	event::id::buf set(const json::array &value) const;
	event::id::buf sort(const sort_closure &) const;
	event::id::buf add(const string_view &) const;
	event::id::buf del(const string_view &) const;

	breadcrumb_rooms(const m::user &user)
	:account_data{user}
	{}
};
