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
	struct events;
	struct profile;
	struct account_data;
	struct room_account_data;
	struct room_tags;
	struct filter;

	using id = m::id::user;
	using closure = std::function<void (const user &)>;
	using closure_bool = std::function<bool (const user &)>;

	static m::room users;
	static m::room tokens;

	id user_id;

	operator const id &() const;

	static string_view gen_access_token(const mutable_buffer &out);
	static id::device::buf get_device_from_access_token(const string_view &token);

	id::room room_id(const mutable_buffer &) const;
	id::room::buf room_id() const;

	bool is_password(const string_view &password) const noexcept;
	event::id::buf password(const string_view &password);

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

	explicit
	room(const m::user &user,
	     const vm::copts *const & = nullptr,
	     const event::fetch::opts *const & = nullptr);

	room(const m::user::id &user_id,
	     const vm::copts *const & = nullptr,
	     const event::fetch::opts *const & = nullptr);

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
	bool for_each(const string_view &membership, const closure_bool &) const;
	void for_each(const string_view &membership, const closure &) const;

	// All rooms with any membership
	bool for_each(const closure_bool &) const;
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
	bool for_each(const string_view &membership, const closure_bool &) const;
	void for_each(const string_view &membership, const closure &) const;
	bool for_each(const closure_bool &) const;
	void for_each(const closure &) const;

	origins(const m::user &user);
};

/// Interface to the other users visible to a user from common rooms.
struct ircd::m::user::mitsein
{
	m::user user;

  public:
	// All common rooms with user
	bool for_each(const m::user &, const string_view &membership, const rooms::closure_bool &) const;
	void for_each(const m::user &, const string_view &membership, const rooms::closure &) const;
	bool for_each(const m::user &, const rooms::closure_bool &) const;
	void for_each(const m::user &, const rooms::closure &) const;

	// All common users in all rooms
	bool for_each(const string_view &membership, const closure_bool &) const;
	void for_each(const string_view &membership, const closure &) const;
	bool for_each(const closure_bool &) const;
	void for_each(const closure &) const;

	// Counting convenience
	size_t count(const m::user &, const string_view &membership = {}) const;
	size_t count(const string_view &membership = {}) const;

	// Existential convenience (does `user` and `other` share any common room).
	bool has(const m::user &other, const string_view &membership = {}) const;

	mitsein(const m::user &user);
};

/// Interface to all events from a user (sender)
struct ircd::m::user::events
{
	using idx_closure_bool = std::function<bool (const event::idx &)>;
	using closure_bool = std::function<bool (const event &)>;

	m::user user;

  public:
	bool for_each(const idx_closure_bool &) const;
	bool for_each(const closure_bool &) const;
	size_t count() const;

	events(const m::user &user);
};

/// Interface to the user profile
struct ircd::m::user::profile
{
	using closure_bool = std::function<bool (const string_view &, const string_view &)>;
	using closure = std::function<void (const string_view &, const string_view &)>;

	m::user user;

	static bool for_each(const m::user &, const closure_bool &);
	static bool get(std::nothrow_t, const m::user &, const string_view &key, const closure &);
	static event::id::buf set(const m::user &, const string_view &key, const string_view &value);

  public:
	bool for_each(const closure_bool &) const;
	bool get(std::nothrow_t, const string_view &key, const closure &) const;
	void get(const string_view &key, const closure &) const;
	string_view get(const mutable_buffer &out, const string_view &key) const; // nothrow
	event::id::buf set(const string_view &key, const string_view &val) const;

	profile(const m::user &user)
	:user{user}
	{}
};

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

struct ircd::m::user::room_account_data
{
	using closure_bool = std::function<bool (const string_view &key, const json::object &)>;
	using closure = std::function<void (const string_view &key, const json::object &)>;

	static constexpr const string_view &type_prefix
	{
		"ircd.account_data"
	};

	static constexpr const size_t &typebuf_size
	{
		m::room::id::MAX_SIZE + size(type_prefix)
	};

	m::user user;
	m::room room;

	static string_view _type(const mutable_buffer &out, const m::room::id &);
	static bool for_each(const m::user &, const m::room &, const closure_bool &);
	static bool get(std::nothrow_t, const m::user &, const m::room &, const string_view &type, const closure &);
	static event::id::buf set(const m::user &, const m::room &, const string_view &type, const json::object &value);

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

struct ircd::m::user::room_tags
{
	using closure_bool = std::function<bool (const string_view &key, const json::object &)>;
	using closure = std::function<void (const string_view &key, const json::object &)>;

	static constexpr const string_view &type_prefix
	{
		"ircd.room_tag"
	};

	static constexpr const size_t &typebuf_size
	{
		m::room::id::MAX_SIZE + size(type_prefix)
	};

	m::user user;
	m::room room;

	static string_view _type(const mutable_buffer &out, const m::room::id &);
	static bool for_each(const m::user &, const m::room &, const closure_bool &);
	static bool get(std::nothrow_t, const m::user &, const m::room &, const string_view &type, const closure &);
	static event::id::buf set(const m::user &, const m::room &, const string_view &type, const json::object &value);
	static bool del(const m::user &, const m::room &, const string_view &type);

  public:
	bool for_each(const closure_bool &) const;
	bool get(std::nothrow_t, const string_view &type, const closure &) const;
	void get(const string_view &type, const closure &) const;
	json::object get(const mutable_buffer &out, const string_view &type) const; //nothrow
	event::id::buf set(const string_view &type, const json::object &value) const;
	bool del(const string_view &type) const;

	room_tags(const m::user &user, const m::room &room)
	:user{user}
	,room{room}
	{}
};

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

inline ircd::m::user::operator
const ircd::m::user::id &()
const
{
	return user_id;
}
