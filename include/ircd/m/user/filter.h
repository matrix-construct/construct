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
#define HAVE_IRCD_M_USER_FILTER_H

struct ircd::m::user::filter
{
	using closure_bool = std::function<bool (const string_view &, const json::object &)>;
	using closure = std::function<void (const string_view &, const json::object &)>;

	m::user user;

	static bool for_each(const m::user &, const closure_bool &);
	static bool get(std::nothrow_t, const m::user &, const string_view &id, const closure &);
	static string_view set(const mutable_buffer &idbuf, const m::user &, const json::object &);

  public:
	bool for_each(const closure_bool &) const;
	bool get(std::nothrow_t, const string_view &filter_id, const closure &) const;
	void get(const string_view &id, const closure &) const;
	json::object get(const mutable_buffer &out, const string_view &id) const; // nothrow
	std::string get(const string_view &id) const; // nothrow
	string_view set(const mutable_buffer &idbuf, const json::object &filter) const;

	filter(const m::user &user)
	:user{user}
	{}
};
