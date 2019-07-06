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

/// Event Fetcher (local).
///
/// Fetches event data from the local database and populates an m::event which
/// is a parent of this class; an instance of this object can be used as an
/// `m::event` via that upcast. The data backing this `m::event` is a zero-copy
/// reference into the database and its lifetime is governed by the internals
/// of this object.
///
/// This can be constructed from either an event::idx or an `event_id`; the
/// latter will incur an extra m::index() lookup. The constructor will throw
/// for a missing event; the nothrow overloads will set a boolean indicator
/// instead. A default constructor can also be used; after construction the
/// seek() ADL suite can be used to the above effect.
///
/// The data is populated by one of two query types to the database; this is
/// determined automatically by default, but can be configured further with
/// the options structure.
///
struct ircd::m::event::fetch
:event
{
	struct opts;

	using keys = event::keys;
	using view_closure = std::function<void (const string_view &)>;

	static const opts default_opts;

	const opts *fopts {&default_opts};
	idx event_idx {0};
	std::array<db::cell, event::size()> cell;
	db::cell _json;
	db::row row;
	bool valid;

	static string_view key(const event::idx *const &);
	static bool should_seek_json(const opts &);
	bool assign_from_row(const string_view &key);
	bool assign_from_json(const string_view &key);

  public:
	fetch(const idx &, std::nothrow_t, const opts & = default_opts);
	fetch(const id &, std::nothrow_t, const opts & = default_opts);
	fetch(const id &, const opts & = default_opts);
	fetch(const idx &, const opts & = default_opts);
	fetch(const opts & = default_opts);
};

namespace ircd::m
{
	bool seek(event::fetch &, const event::idx &, std::nothrow_t);
	void seek(event::fetch &, const event::idx &);

	bool seek(event::fetch &, const event::id &, std::nothrow_t);
	void seek(event::fetch &, const event::id &);
}

/// Event Fetch Options.
///
/// Refer to the individual member documentations for details. Notes:
///
/// - The default keys selection is *all keys*. This is unnecessarily
/// expensive I/O for most uses of event::fetch; consider narrowing the keys
/// selection based on what properties of the `m::event` will be accessed.
///
/// - Row Query: The event is populated by conducting a set of point lookups
/// for the selected keys. The point lookups are parallelized so the latency
/// of a lookup is only limited to the slowest key. The benefit of this type
/// is that very efficient I/O and caching can be conducted, but the cost is
/// that each lookup in the row occupies a hardware I/O lane which is a limited
/// resource shared by the whole system.
///
/// - JSON Query: The event is populated by conducting a single point lookup
/// to a database value containing the full JSON string of the event. This
/// query is made when all keys are selected. The benefit of this query is that
/// it only occupies one hardware I/O lane in contrast with the row query. The
/// cost is that the full event JSON is read from storage (up to 64_KiB) and
/// maintained in cache.
///
struct ircd::m::event::fetch::opts
{
	/// Event property selector
	event::keys::selection keys;

	/// Database get options passthru
	db::gopts gopts;

	/// Whether to force an attempt at populating the event from event_json
	/// first, bypassing any decision-making. This is useful if a key selection
	/// is used which would trigger a row query but the developer wants the
	/// json query anyway.
	bool query_json_force {false};

	opts(const event::keys::selection &, const db::gopts & = {});
	opts(const db::gopts &, const event::keys::selection & = {});
	opts() = default;
};
