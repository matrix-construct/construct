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
enum class ircd::m::dbs::ref
:uint8_t
{
	// DAG
	PREV                = 0x00,
	AUTH                = 0x01,
	STATE               = 0x02,
	PREV_STATE          = 0x04,

	// m.receipt
	M_RECEIPT__M_READ   = 0x10,

	// m.relates_to
	M_RELATES__M_REPLY  = 0x20,

	// m.room.redaction
	M_ROOM_REDACTION    = 0x40,
};
