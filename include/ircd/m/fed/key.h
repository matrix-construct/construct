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
#define HAVE_IRCD_M_FED_KEY_H

namespace ircd::m::fed::key
{
	struct keys;
	struct query;

	using opts = request::opts;
	using server_key = std::pair<string_view, string_view>; // server_name, key_id
};

struct ircd::m::fed::key::keys
:request
{
	explicit operator json::object() const
	{
		return json::object
		{
			in.content
		};
	}

	keys(const server_key &,
	     const mutable_buffer &,
	     opts);

	keys(const string_view &server_name,
	     const mutable_buffer &,
	     opts);

	keys() = default;
};

struct ircd::m::fed::key::query
:request
{
	explicit operator json::array() const
	{
		const json::object object
		{
			in.content
		};

		const json::array server_keys
		{
			object["server_keys"]
		};

		return server_keys;
	}

	query(const vector_view<const server_key> &,
	      const mutable_buffer &,
	      opts);

	query() = default;
};
