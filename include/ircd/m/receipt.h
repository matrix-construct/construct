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
	// [GET]
	bool exists(const id::room &, const id::user &, const id::event &);
	bool freshest(const id::room &, const id::user &, const id::event &);
	bool ignoring(const m::user &, const id::event &);
	bool ignoring(const m::user &, const id::room &);
	bool read(const id::room &, const id::user &, const id::event::closure &);
	id::event read(id::event::buf &out, const id::room &, const id::user &);

	// [SET]
	id::event::buf read(const id::room &, const id::user &, const id::event &, const time_t &);
	id::event::buf read(const id::room &, const id::user &, const id::event &); // now
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
