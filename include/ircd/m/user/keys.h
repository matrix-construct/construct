// The Construct
//
// Copyright (C) The Construct Developers, Authors & Contributors
// Copyright (C) 2016-2023 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

#pragma once
#define HAVE_IRCD_M_USER_KEYS_H

struct ircd::m::user::keys
{
	static string_view make_sigs_state_key(const mutable_buffer &, const string_view &tgt, const string_view &src);
	static std::tuple<string_view, string_view> unmake_sigs_state_key(const string_view &) noexcept;

	m::user::room user_room;

	void attach_sigs(json::stack::object &, const json::object &) const;
	bool attach_sigs(json::stack::object &, const event::idx &) const;

  public:
	void device(json::stack::object &, const string_view &device_id) const;
	void cross_master(json::stack::object &) const;
	void cross_self(json::stack::object &) const;
	void cross_user(json::stack::object &) const;

	keys(const m::user &user)
	:user_room{user}
	{}
};
