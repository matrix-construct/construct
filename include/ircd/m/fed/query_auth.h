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
#define HAVE_IRCD_M_FED_QUERY_AUTH_H

namespace ircd::m::fed
{
	struct query_auth;
};

struct ircd::m::fed::query_auth
:request
{
	explicit operator json::object() const
	{
		const auto type
		{
			json::type(in.content)
		};

		return type == json::ARRAY?
			json::array{in.content}.at(1):  // non-spec [200, {...}]
			json::object{in.content};       // spec {...}
	}

	query_auth(const m::room::id &,
	           const m::event::id &,
	           const json::object &content,
	           const mutable_buffer &,
	           opts);

	query_auth() = default;
};
