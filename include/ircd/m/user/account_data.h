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
#define HAVE_IRCD_M_USER_ACCOUNT_DATA_H

struct ircd::m::user::account_data
{
	using closure_bool = std::function<bool (const string_view &key, const json::object &)>;
	using closure = std::function<void (const string_view &key, const json::object &)>;

	m::user user;

	static bool for_each(const m::user &, const closure_bool &);
	static bool get(std::nothrow_t, const m::user &, const string_view &type, const closure &);
	static event::id::buf set(const m::user &, const string_view &type, const json::object &value);

  public:
	bool for_each(const closure_bool &) const;
	bool get(std::nothrow_t, const string_view &type, const closure &) const;
	void get(const string_view &type, const closure &) const;
	json::object get(const mutable_buffer &out, const string_view &type) const; //nothrow
	event::id::buf set(const string_view &type, const json::object &value) const;

	account_data(const m::user &user)
	:user{user}
	{}
};
