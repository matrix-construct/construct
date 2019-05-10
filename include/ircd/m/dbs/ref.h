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
#define HAVE_IRCD_M_DBS_REF_H

/// Types of references indexed by event_refs. This is a single byte integer,
/// which should be plenty of namespace. Internally event_refs_key() and
/// event_refs store this in a high order byte of an event::idx integer. This
/// is an alternative to having separate columns for each type of reference.
///
/// NOTE: These values are written to the database and cannot be changed.
enum class ircd::m::dbs::ref
:uint8_t
{
	/// All events which reference this event in their `prev_events`.
	NEXT                = 0x00,

	/// All power events which reference this event in their `auth_events`.
	/// Non-auth/non-power events are not involved in this graph at all.
	NEXT_AUTH           = 0x01,

	/// The next states in the transitions for a (type,state_key) cell.
	NEXT_STATE          = 0x02,

	/// The previous states in the transitions for a (type,state_key) cell.
	PREV_STATE          = 0x04,

	/// All m.receipt's which target this event.
	M_RECEIPT__M_READ   = 0x10,

	/// All m.relates_to's which target this event.
	M_RELATES__M_REPLY  = 0x20,

	/// All m.room.redaction's which target this event.
	M_ROOM_REDACTION    = 0x40,
};
