// Matrix Construct
//
// Copyright (C) Matrix Construct Developers, Authors & Contributors
// Copyright (C) 2016-2023 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

#pragma once
#define HAVE_IRCD_M_ROOM_PURGE_H

/// Erase the room from the database. Cuidado!
///
/// The room purge is an application of multiple event::purge operations.
/// By default the entire room is purged. The options can tweak specifics.
///
struct ircd::m::room::purge
:returns<size_t>
{
	struct opts;

	static const struct opts opts_default;
	static log::log log;

  private:
	const m::room &room;
	const struct opts &opts;
	db::txn txn;

	bool match(const event::idx &, const event &) const;
	bool match(const uint64_t &, const event::idx &) const;
	void timeline();
	void state();
	void commit();

  public:
	purge(const m::room &, const struct opts & = opts_default);
};

/// Options for purge
struct ircd::m::room::purge::opts
{
	/// Limit purge to the index window
	pair<event::idx> idx
	{
		0, std::numeric_limits<event::idx>::max()
	};

	/// Limit purge to the depth window
	pair<uint64_t> depth
	{
		0, std::numeric_limits<uint64_t>::max()
	};

	/// Limit purge to events matching the filter
	const m::event_filter *filter {nullptr};

	/// Set to false to not purge any state events.
	bool state {true};

	/// Set to false to not purge the present state, but prior (replaced)
	/// states will be purged if other options permit.
	bool present {true};

	/// Set to false to not purge replaced states; the only state events
	/// considered for purge are present states if other options permit.
	bool history {true};

	/// Timeline in this context refers to non-state events. Set to false to
	/// only allow state events to be purged; true to allow non-state events
	/// if other options permit.
	bool timeline {true};

	/// Log an INFO message for the final transaction; takes precedence
	/// if both debuglog and infolog are true.
	bool infolog_txn {false};

	/// Log a DEBUG message for the final transaction.
	bool debuglog_txn {true};
};
