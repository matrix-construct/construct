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
#define HAVE_IRCD_M_RECEIPT_H

namespace ircd::m::receipt
{
	// [GET] Query if the user has ever read the event.
	bool exists(const id::room &, const id::user &, const id::event &);

	// [GET] Query if already read a later event in the room; false if so.
	bool freshest(const id::room &, const id::user &, const id::event &);

	// [GET] Query if the user is not *sending* receipts for certain events
	// matched internal features of this interface (i.e by sender).
	bool ignoring(const m::user &, const id::event &);

	// [GET] Query if the user is not *sending* receipts to an entire room.
	bool ignoring(const m::user &, const id::room &);

	// [GET] Get the last event the user has read in the room.
	bool get(const id::room &, const id::user &, const id::event::closure &);
	id::event get(id::event::buf &out, const id::room &, const id::user &);

	// [SET] Indicate that the user has read the event in the room.
	id::event::buf read(const id::room &, const id::user &, const id::event &, const json::object & = {});

	extern log::log log;
};

struct ircd::m::edu::m_receipt
{
	struct m_read;
};

struct ircd::m::edu::m_receipt::m_read
:json::tuple
<
	json::property<name::data, json::object>,
	json::property<name::event_ids, json::array>
>
{
	using super_type::tuple;
	using super_type::operator=;
};

inline ircd::m::event::id
ircd::m::receipt::get(event::id::buf &out,
                      const room::id &room_id,
                      const user::id &user_id)
{
	const event::id::closure copy
	{
		[&out](const auto &event_id) { out = event_id; }
	};

	return get(room_id, user_id, copy)? event::id{out} : event::id{};
}
