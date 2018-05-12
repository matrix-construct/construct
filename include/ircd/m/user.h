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

	bool my(const user &);
	bool exists(const user &);
	bool exists(const id::user &);
	user create(const id::user &, const json::members &args = {});
}

struct ircd::m::user
{
	struct room;
	struct rooms;
	struct mitsein;
	using id = m::id::user;
	using closure = std::function<void (const user &)>;
	using closure_bool = std::function<bool (const user &)>;

	static m::room users;
	static m::room tokens;

	id user_id;

	operator const id &() const;

	static string_view gen_access_token(const mutable_buffer &out);

	id::room room_id(const mutable_buffer &) const;
	id::room::buf room_id() const;

	bool is_password(const string_view &password) const noexcept;
	event::id::buf password(const string_view &password);

	using profile_closure = std::function<void (const string_view &)>;
	void profile(const string_view &key, const profile_closure &) const;
	bool profile(std::nothrow_t, const string_view &key, const profile_closure &) const;
	string_view profile(const mutable_buffer &out, const string_view &key) const; //nothrow
	event::id::buf profile(const m::user &sender, const string_view &key, const string_view &value);

	using account_data_closure = std::function<void (const json::object &)>;
	void account_data(const string_view &type, const account_data_closure &) const;
	bool account_data(std::nothrow_t, const string_view &type, const account_data_closure &) const;
	json::object account_data(const mutable_buffer &out, const string_view &type) const; //nothrow
	event::id::buf account_data(const m::user &sender, const string_view &type, const json::object &value);

	using filter_closure = std::function<void (const json::object &)>;
	bool filter(std::nothrow_t, const string_view &filter_id, const filter_closure &) const;
	void filter(const string_view &filter_id, const filter_closure &) const;
	std::string filter(std::nothrow_t, const string_view &filter_id) const;
	std::string filter(const string_view &filter_id) const;
	event::id::buf filter(const json::object &filter, const mutable_buffer &id);

	bool is_active() const;
	event::id::buf deactivate();
	event::id::buf activate();

	user(const id &user_id)
	:user_id{user_id}
	{}

	user() = default;
};

struct ircd::m::user::room
:m::room
{
	m::user user;
	id::room::buf room_id;

	room(const m::user &user);
	room(const m::user::id &user_id);
	room() = default;
	room(const room &) = delete;
	room &operator=(const room &) = delete;
};

/// Interface to the rooms for a user.
struct ircd::m::user::rooms
{
	struct origins;

	using closure = std::function<void (const m::room &, const string_view &)>;
	using closure_bool = std::function<bool (const m::room &, const string_view &)>;

	m::user::room user_room;

  public:
	// All rooms with specific membership
	void for_each(const string_view &membership, const closure_bool &) const;
	void for_each(const string_view &membership, const closure &) const;

	// All rooms with any membership
	void for_each(const closure_bool &) const;
	void for_each(const closure &) const;

	size_t count(const string_view &membership) const;
	size_t count() const;

	rooms(const m::user &user);
};

/// Interface to the other servers visible to a user from all their rooms
struct ircd::m::user::rooms::origins
{
	using closure = m::room::origins::closure;
	using closure_bool = m::room::origins::closure_bool;

	m::user user;

  public:
	void for_each(const string_view &membership, const closure_bool &) const;
	void for_each(const string_view &membership, const closure &) const;
	void for_each(const closure_bool &) const;
	void for_each(const closure &) const;

	origins(const m::user &user);
};

/// Interface to the other users visible to a user from common rooms.
struct ircd::m::user::mitsein
{
	m::user user;

  public:
	// All common rooms with user
	void for_each(const m::user &, const string_view &membership, const rooms::closure_bool &) const;
	void for_each(const m::user &, const string_view &membership, const rooms::closure &) const;
	void for_each(const m::user &, const rooms::closure_bool &) const;
	void for_each(const m::user &, const rooms::closure &) const;

	// All common users in all rooms
	void for_each(const string_view &membership, const closure_bool &) const;
	void for_each(const string_view &membership, const closure &) const;
	void for_each(const closure_bool &) const;
	void for_each(const closure &) const;

	size_t count(const m::user &, const string_view &membership = {}) const;
	size_t count(const string_view &membership = {}) const;

	mitsein(const m::user &user);
};

inline ircd::m::user::operator
const ircd::m::user::id &()
const
{
	return user_id;
}
