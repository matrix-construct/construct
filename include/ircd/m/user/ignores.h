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
#define HAVE_IRCD_M_USER_IGNORES_H

/// Interface to the user ignorance
struct ircd::m::user::ignores
{
	using closure_bool = std::function<bool (const m::user::id &, const json::object &)>;

	m::user user;

	static bool enforce(const string_view &type);

  public:
	bool for_each(const closure_bool &) const;
	bool has(const m::user::id &other) const;

	ignores(const m::user &user)
	:user{user}
	{}
};
