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

namespace ircd::m
{
	bool seek(event::fetch &, const event::idx &, std::nothrow_t);
	void seek(event::fetch &, const event::idx &);

	bool seek(event::fetch &, const event::id &, std::nothrow_t);
	void seek(event::fetch &, const event::id &);
}

struct ircd::m::event::fetch
:event
{
	struct opts;

	using keys = event::keys;
	using view_closure = std::function<void (const string_view &)>;

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
