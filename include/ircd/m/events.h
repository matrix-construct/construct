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
#define HAVE_IRCD_M_EVENTS_H

namespace ircd::m::events
{
	struct range;
	using closure = std::function<bool (const event::idx &, const event &)>;

	// Iterate events in range
	bool for_each(const range &, const closure &);

	// Iterate events in range w/ filter
	bool for_each(const range &, const event_filter &, const closure &);
	bool for_each(const range &, const event_filter &, const event::closure_idx_bool &);

	// util
	void dump__file(const string_view &filename);
	void rebuild();
}

/// Interface to the types of all events known to this server.
namespace ircd::m::events::type
{
	using closure = std::function<bool (const string_view &, const event::idx &)>;
	using closure_name = std::function<bool (const string_view &)>;

	// Iterate the names of all event types.
	bool for_each(const closure_name &);
	bool for_each(const string_view &prefix, const closure_name &);

	// Iterate the events for a specific type.
	bool for_each_in(const string_view &, const closure &);

	// Test if type name is known to the server.
	bool has_prefix(const string_view &);
	bool has(const string_view &);
}

/// Interface to the senders of all events known to the server.
namespace ircd::m::events::sender
{
	using closure = std::function<bool (const id::user &, const event::idx &)>;
	using closure_name = std::function<bool (const id::user &)>;

	// Iterate all of the sender mxids known to the server.
	bool for_each(const closure_name &);
	bool for_each(const string_view &key, const closure_name &);

	// Iterate all of the events for a specific sender mxid.
	bool for_each_in(const id::user &, const closure &);
}

/// Interface to the servers of the senders of all events known to this server.
namespace ircd::m::events::origin
{
	using closure_name = std::function<bool (const string_view &)>;

	// Iterate all server names known to this server.
	bool for_each(const closure_name &);
	bool for_each(const string_view &hostlb, const closure_name &);

	// Iterate all senders mxids on a specific server.
	bool for_each_in(const string_view &, const sender::closure &);
}

/// Interface to the state_key index of all events known to this server.
namespace ircd::m::events::state
{
	using tuple = dbs::event_state_tuple;
	using closure = std::function<bool (const tuple &)>;

	bool for_each(const tuple &, const closure &);
	bool for_each(const closure &);
}

/// Interface to scan the refs of events
namespace ircd::m::events::refs
{
	// (source, type, target)
	using closure = std::function<bool (const event::idx &, const dbs::ref &, const event::idx &)>;

	// Iterate events in range
	bool for_each(const range &, const closure &);
}

/// Interface to scan the content of events
namespace ircd::m::events::content
{
	using closure = std::function<bool (const event::idx &, const json::object &)>;

	bool for_each(const closure &);
}

/// Interface to scan the JSON source of events
namespace ircd::m::events::source
{
	using closure = std::function<bool (const event::idx &, const json::object &)>;

	// Iterate events in range
	bool for_each(const range &, const closure &);
}

/// Range to start (inclusive) and stop (exclusive). If start is greater than
/// stop a reverse iteration will occur. -1 (or unsigned max value) can be used
/// to start or stop at the end. 0 can be used to start or stop at the beginning.
/// (event::idx of 0 is a sentinel)
///
struct ircd::m::events::range
:event::idx_range
{
	const event::fetch::opts *fopts {nullptr};

	range(const event::idx &start,
	      const event::idx &stop = -1,
	      const event::fetch::opts *const &fopts = nullptr)
	:event::idx_range{start, stop}
	,fopts{fopts}
	{}
};

inline bool
ircd::m::events::origin::for_each(const closure_name &closure)
{
	return for_each(string_view{}, closure);
}

inline bool
ircd::m::events::sender::for_each(const closure_name &closure)
{
	return for_each(string_view{}, closure);
}

inline bool
ircd::m::events::type::for_each(const closure_name &closure)
{
	return for_each(string_view{}, closure);
}
