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
	struct opts;
	struct result;
	using closure = std::function<bool (const result &)>;

	bool head(const opts &, const closure &);
	bool state(const opts &, const closure &);
	bool version(const opts &, const closure &);
};

struct ircd::m::feds::result
{
	string_view origin;
	std::exception_ptr eptr;
	json::object object;
	json::array array;
};

struct ircd::m::feds::opts
{
	milliseconds timeout {20000L};
	m::room::id room_id;
	m::event::id event_id;
	m::user::id user_id;
	bool ids {false};
};
