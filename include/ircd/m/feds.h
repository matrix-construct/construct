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
#define HAVE_IRCD_M_FEDS_H

namespace ircd::m::feds
{
	struct head;
	struct state;
}

struct ircd::m::feds::state
{
	using closure = std::function<bool (const string_view &, std::exception_ptr, const json::object &)>;

	state(const m::room::id &, const m::event::id &, const milliseconds &, const closure &);
	state(const m::room::id &, const m::event::id &, const closure &);
};

struct ircd::m::feds::head
{
	using closure = std::function<bool (const string_view &, std::exception_ptr, const json::object &)>;

	head(const m::room::id &, const m::user::id &, const milliseconds &, const closure &);
	head(const m::room::id &, const m::user::id &, const closure &);
	head(const m::room::id &, const closure &);
};
