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
#define HAVE_IRCD_M_ROOM_MEMBERS_H

/// Interface to the aliases
///
/// This interface focuses specifically on room aliases. These are aliases
/// contained in a room's state. There is also an aliases::cache which stores
/// the result of directory lookups as well as the contents found through this
/// interface in order to resolve aliases to room_ids.
///
struct ircd::m::room::aliases
{
	struct cache;
	using closure_bool = std::function<bool (const alias &)>;

	static bool for_each(const m::room &, const string_view &server, const closure_bool &);

	m::room room;

  public:
	bool for_each(const string_view &server, const closure_bool &) const;
	bool for_each(const closure_bool &) const;
	bool has(const alias &) const;
	size_t count(const string_view &server) const;
	size_t count() const;

	aliases(const m::room &room)
	:room{room}
	{}
};

struct ircd::m::room::aliases::cache
{
	struct entity;
	using closure_bool = std::function<bool (const alias &, const id &)>;

	static string_view make_key(const mutable_buffer &, const alias &);
	static event::idx getidx(const alias &); // nothrow
	static milliseconds age(const event::idx &); // nothrow
	static bool expired(const event::idx &); // nothrow

  public:
	static system_point expires(const alias &);
	static bool has(const alias &);

	static bool for_each(const string_view &server, const closure_bool &);
	static bool for_each(const closure_bool &);

	static void fetch(const alias &, const string_view &remote);
	static bool fetch(std::nothrow_t, const alias &, const string_view &remote);

	static bool get(std::nothrow_t, const alias &, const id::closure &);
	static void get(const alias &, const id::closure &);
	static id::buf get(std::nothrow_t, const alias &);
	static id::buf get(const alias &);

	static bool set(const alias &, const id &);

	static bool del(const alias &);
};
