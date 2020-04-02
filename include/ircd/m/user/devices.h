// The Construct
//
// Copyright (C) The Construct Developers, Authors & Contributors
// Copyright (C) 2016-2020 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

#pragma once
#define HAVE_IRCD_M_USER_DEVICES_H

struct ircd::m::user::devices
{
	using closure = std::function<void (const event::idx &, const string_view &)>;
	using closure_bool = std::function<bool (const event::idx &, const string_view &)>;

	m::user user;

	bool for_each(const closure_bool &) const; // each device_id
	bool for_each(const string_view &id, const closure_bool &) const; // each property

	bool has(const string_view &id, const string_view &prop) const;
	bool has(const string_view &id) const;

	bool get(std::nothrow_t, const string_view &id, const string_view &prop, const closure &) const;
	bool get(const string_view &id, const string_view &prop, const closure &) const;

	bool put(const string_view &id, const string_view &prop, const string_view &val) const;
	bool set(const string_view &id, const string_view &prop, const string_view &val) const;
	bool set(const device &) const;

	bool del(const string_view &id) const;

	///TODO: XXX junk
	static std::map<std::string, long> count_one_time_keys(const m::user &, const string_view &);
	static bool update(const device_list_update &);
	static bool send(json::iov &content);

	devices(const m::user &user)
	:user{user}
	{}
};
