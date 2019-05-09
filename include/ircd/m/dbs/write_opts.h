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
#define HAVE_IRCD_M_DBS_WRITE_OPTS_H

/// Options that affect the dbs::write() of an event to the transaction.
struct ircd::m::dbs::write_opts
{
	static const std::bitset<256> event_refs_all;
	static const std::bitset<64> appendix_all;

	/// Operation code; usually SET or DELETE. Note that we interpret the
	/// code internally and may set different codes for appendages of the
	/// actual transaction.
	db::op op {db::op::SET};

	/// Principal's index number. Most codepaths do not permit zero; must set.
	uint64_t event_idx {0};

	/// The state btree root to perform the update on.
	string_view root_in;

	/// After the update is performed, the new state btree root is returned
	/// into this buffer.
	mutable_buffer root_out;

	/// Fuse panel to toggle transaction elements.
	std::bitset<64> appendix {appendix_all};

	/// Selection of what reference types to manipulate in event_refs. Refs
	/// will not be made if it is not appropriate for the event anyway, so
	/// this defaults to all bits. User can disable one or more ref types
	/// by clearing a bit.
	std::bitset<256> event_refs {event_refs_all};

	/// Selection of what reference types to resolve and delete from the
	/// event_horizon for this event.
	std::bitset<256> horizon_resolve {event_refs_all};

	/// Whether the present state table `room_state` should be updated by
	/// this operation if appropriate.
	bool present {true};

	/// Whether the history state btree `state_node` + `room_events` value
	/// should be updated by this operation if appropriate.
	bool history {false};

	/// Whether the event.source can be used directly for event_json. Defaults
	/// to false unless the caller wants to avoid a redundant re-stringify.
	bool json_source {false};
};
