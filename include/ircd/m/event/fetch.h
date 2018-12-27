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
#define HAVE_IRCD_M_EVENT_FETCH_H

struct ircd::m::event::fetch
:event
{
	struct opts;
	using keys = event::keys;

	static const opts default_opts;

	std::array<db::cell, event::size()> cell;
	db::row row;
	bool valid;

  public:
	fetch(const idx &, std::nothrow_t, const opts *const & = nullptr);
	fetch(const idx &, const opts *const & = nullptr);
	fetch(const id &, std::nothrow_t, const opts *const & = nullptr);
	fetch(const id &, const opts *const & = nullptr);
	fetch(const opts *const & = nullptr);

	static bool event_id(const idx &, std::nothrow_t, const id::closure &);
	static void event_id(const idx &, const id::closure &);

	friend bool index(const id &, std::nothrow_t, const closure_idx &);
	friend idx index(const id &, std::nothrow_t);
	friend idx index(const id &);
	friend idx index(const event &, std::nothrow_t);
	friend idx index(const event &);

	friend bool seek(fetch &, const idx &, std::nothrow_t);
	friend void seek(fetch &, const idx &);
	friend bool seek(fetch &, const id &, std::nothrow_t);
	friend void seek(fetch &, const id &);

	using view_closure = std::function<void (const string_view &)>;
	friend bool get(std::nothrow_t, const idx &, const string_view &key, const view_closure &);
	friend bool get(std::nothrow_t, const id &, const string_view &key, const view_closure &);
	friend void get(const idx &, const string_view &key, const view_closure &);
	friend void get(const id &, const string_view &key, const view_closure &);

	friend const_buffer get(std::nothrow_t, const idx &, const string_view &key, const mutable_buffer &out);
	friend const_buffer get(std::nothrow_t, const id &, const string_view &key, const mutable_buffer &out);
	friend const_buffer get(const idx &, const string_view &key, const mutable_buffer &out);
	friend const_buffer get(const id &, const string_view &key, const mutable_buffer &out);

	friend bool cached(const idx &, const opts & = default_opts);
	friend bool cached(const id &, const opts &);
	friend bool cached(const id &);

	friend void prefetch(const idx &, const string_view &key);
	friend void prefetch(const idx &, const opts & = default_opts);
	friend void prefetch(const id &, const string_view &key);
	friend void prefetch(const id &, const opts & = default_opts);
};

struct ircd::m::event::fetch::opts
{
	event::keys keys;
	db::gopts gopts;

	opts(const event::keys &, const db::gopts & = {});
	opts(const event::keys::selection &, const db::gopts & = {});
	opts(const db::gopts &, const event::keys::selection & = {});
	opts() = default;
};
