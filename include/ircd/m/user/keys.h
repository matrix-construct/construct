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
	struct send;

	static string_view make_sigs_state_key(const mutable_buffer &, const string_view &tgt, const string_view &src);
	static std::tuple<string_view, string_view> unmake_sigs_state_key(const string_view &) noexcept;

	m::user::room user_room;

	void attach_sigs(json::stack::object &, const json::object &, const user::id &) const;
	bool attach_sigs(json::stack::object &, const event::idx &, const user::id &) const;
	void append_keys(json::stack::object &, const json::object &, const user::id &) const;
	bool append_keys(json::stack::object &, const event::idx &, const user::id &) const;

  public:
	bool has_device(const m::id::device &) const;
	bool has_cross_master() const;
	bool has_cross_self() const;
	bool has_cross_user() const;

	void device(json::stack::object &, const string_view &device_id) const;
	void cross_master(json::stack::object &) const;
	void cross_self(json::stack::object &) const;
	void cross_user(json::stack::object &) const;

	void update(const m::signing_key_update &) const;

	keys(const m::user &user)
	:user_room{user}
	{}
};

struct ircd::m::user::keys::send
{
	send(const m::user::keys &,
	     const string_view = {});
};

inline void
ircd::m::user::keys::cross_user(json::stack::object &out)
const
{
	const auto event_idx
	{
		user_room.get(std::nothrow, "ircd.cross_signing.user", "")
	};

	append_keys(out, event_idx, user_room.user.user_id);
}

inline void
ircd::m::user::keys::cross_self(json::stack::object &out)
const
{
	const auto event_idx
	{
		user_room.get(std::nothrow, "ircd.cross_signing.self", "")
	};

	append_keys(out, event_idx, user_room.user.user_id);
}

inline void
ircd::m::user::keys::cross_master(json::stack::object &out)
const
{
	const auto event_idx
	{
		user_room.get(std::nothrow, "ircd.cross_signing.master", "")
	};

	append_keys(out, event_idx, user_room.user.user_id);
}

inline bool
ircd::m::user::keys::has_cross_user()
const
{
	return user_room.has("ircd.cross_signing.user", "");
}

inline bool
ircd::m::user::keys::has_cross_self()
const
{
	return user_room.has("ircd.cross_signing.self", "");
}

inline bool
ircd::m::user::keys::has_cross_master()
const
{
	return user_room.has("ircd.cross_signing.master", "");
}

inline bool
ircd::m::user::keys::has_device(const m::id::device &device_id)
const
{
	const m::user::devices devices{user_room.user};
	return devices.has(device_id, "keys");
}
