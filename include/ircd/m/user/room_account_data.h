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
#define HAVE_IRCD_M_USER_ROOM_ACCOUNT_DATA_H

struct ircd::m::user::room_account_data
{
	using closure_bool = std::function<bool (const string_view &key, const json::object &)>;
	using closure = std::function<void (const string_view &key, const json::object &)>;

	static constexpr const string_view &type_prefix
	{
		"ircd.account_data"
	};

	static constexpr const size_t typebuf_size
	{
		m::room::id::MAX_SIZE + 24
	};

	m::user user;
	m::room room;

	static string_view _type(const mutable_buffer &out, const m::room::id &);

  public:
	bool for_each(const closure_bool &) const;
	bool get(std::nothrow_t, const string_view &type, const closure &) const;
	void get(const string_view &type, const closure &) const;
	json::object get(const mutable_buffer &out, const string_view &type) const; //nothrow
	event::id::buf set(const string_view &type, const json::object &value) const;

	room_account_data(const m::user &user, const m::room &room)
	:user{user}
	,room{room}
	{}
};
