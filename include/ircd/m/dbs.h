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
#define HAVE_IRCD_M_DBS_H

namespace ircd::m::dbs
{
	struct init;
	struct write_opts;

	// Database instance
	extern std::shared_ptr<db::database> events;

	// Event property columns
	constexpr const auto event_columns{event::size()};
	extern std::array<db::column, event_columns> event_column;

	// Event metadata columns
	extern db::column state_node;
	extern db::index room_events;
	extern db::index room_origins;
	extern db::index room_state;

	// Lowlevel util
	string_view room_state_key(const mutable_buffer &out, const id::room &, const string_view &type, const string_view &state_key);
	string_view room_state_key(const mutable_buffer &out, const id::room &, const string_view &type);
	std::tuple<string_view, string_view> room_state_key(const string_view &amalgam);

	string_view room_origins_key(const mutable_buffer &out, const id::room &, const string_view &origin, const id::user &member);
	std::tuple<string_view, string_view> room_origins_key(const string_view &amalgam);

	string_view room_events_key(const mutable_buffer &out, const id::room &, const uint64_t &depth, const id::event &);
	string_view room_events_key(const mutable_buffer &out, const id::room &, const uint64_t &depth);
	std::tuple<uint64_t, string_view> room_events_key(const string_view &amalgam);

	// Get the state root for an event (with as much information as you have)
	string_view state_root(const mutable_buffer &out, const id::room &, const id::event &, const uint64_t &depth);
	string_view state_root(const mutable_buffer &out, const id::room &, const id::event &);
	string_view state_root(const mutable_buffer &out, const id::event &);
	string_view state_root(const mutable_buffer &out, const event &);

	// [SET (txn)] Basic write suite
	string_view write(db::txn &, const event &, const write_opts &);
}

struct ircd::m::dbs::write_opts
{
	string_view root_in;
	mutable_buffer root_out;
	bool present {true};
	bool history {true};
};

namespace ircd::m::dbs::desc
{
	// Full description
	extern const database::description events;

	// Direct columns
	extern const database::descriptor events_auth_events;
	extern const database::descriptor events_content;
	extern const database::descriptor events_depth;
	extern const database::descriptor events_event_id;
	extern const database::descriptor events_hashes;
	extern const database::descriptor events_membership;
	extern const database::descriptor events_origin;
	extern const database::descriptor events_origin_server_ts;
	extern const database::descriptor events_prev_events;
	extern const database::descriptor events_prev_state;
	extern const database::descriptor events_redacts;
	extern const database::descriptor events_room_id;
	extern const database::descriptor events_sender;
	extern const database::descriptor events_signatures;
	extern const database::descriptor events_state_key;
	extern const database::descriptor events_type;

	// Metadata columns
	extern const database::descriptor events__state_node;

	extern const db::prefix_transform events__room_events__pfx;
	extern const db::comparator events__room_events__cmp;
	extern const database::descriptor events__room_events;

	extern const db::prefix_transform events__room_origins__pfx;
	extern const database::descriptor events__room_origins;

	extern const db::prefix_transform events__room_state__pfx;
	extern const database::descriptor events__room_state;
}

struct ircd::m::dbs::init
{
	init();
	~init() noexcept;
};
