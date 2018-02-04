// Matrix Construct
//
// Copyright (C) Matrix Construct Developers, Authors & Contributors
// Copyright (C) 2016-2018 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

#pragma once
#define HAVE_IRCD_M_USER_H

namespace ircd::m
{
	struct user;
}

struct ircd::m::user
{
	using id = m::id::user;

	id user_id;

	static room accounts;
	static room sessions;

	bool is_active() const;
	bool is_password(const string_view &password) const;

	void password(const string_view &password);
	void activate(const json::members &contents = {});
	void deactivate(const json::members &contents = {});

	user(const id &user_id)
	:user_id{user_id}
	{}
};
