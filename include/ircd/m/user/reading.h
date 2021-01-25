// The Construct
//
// Copyright (C) The Construct Developers, Authors & Contributors
// Copyright (C) 2016-2021 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

#pragma once
#define HAVE_IRCD_M_USER_READING_H

struct ircd::m::user::reading
{
	/// The user is currently viewing this room (any device).
	id::room::buf room_id;

	/// The user hasn't seen events later than this in the room (m.read)
	id::event::buf last;

	/// The client claims the user saw `last` at this time.
	milliseconds last_ts {0ms};

	/// The client reported `last` at this time.
	milliseconds last_ots {0ms};

	/// The user has fully read up to this event (m.fully_read)
	id::event::buf full;

	/// The client reported `full` at this time
	milliseconds full_ots {0ms};

	/// User is currently_active.
	bool currently_active {false};

	reading(const user &);
	reading() = default;
};
