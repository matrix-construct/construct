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
#define HAVE_IRCD_M_FED_QUERY_H

namespace ircd::m::fed
{
	struct query;
};

struct ircd::m::fed::query
:request
{
	struct profile;
	struct directory;
	struct user_devices;
	struct client_keys;

	explicit operator json::object() const
	{
		return json::object
		{
			in.content
		};
	}

	query(const string_view &type,
	      const string_view &args,
	      const mutable_buffer &,
	      opts);

	query() = default;
};

struct ircd::m::fed::query::profile
:query
{
	profile(const id::user &user_id,
	        const string_view &field,
	        const mutable_buffer &,
	        opts);

	profile(const id::user &user_id,
	        const mutable_buffer &,
	        opts);

	profile() = default;
};

struct ircd::m::fed::query::directory
:query
{
	directory(const id::room_alias &room_alias,
	          const mutable_buffer &,
	          opts);

	directory() = default;
};
