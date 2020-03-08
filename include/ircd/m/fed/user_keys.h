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
#define HAVE_IRCD_M_FED_USER_H

namespace ircd::m::fed::user::keys
{
	struct claim;
	struct query;
}

struct ircd::m::fed::user::keys::query
:request
{
	using opts = fed::user::opts;
	using devices = vector_view<const string_view>;
	using user_devices = std::pair<m::user::id, devices>;
	using users_devices = const vector_view<const user_devices>;
	using users_devices_map = std::map<m::user::id, json::array>;

	static json::object make_content(const mutable_buffer &, const users_devices &);
	static json::object make_content(const mutable_buffer &, const users_devices_map &);

	explicit operator json::object() const
	{
		return json::object
		{
			in.content
		};
	}

	explicit
	query(const json::object &content,
	      const mutable_buffer &,
	      opts);

	query(const users_devices_map &,
	      const mutable_buffer &,
	      opts);

	query(const users_devices &,
	      const mutable_buffer &,
	      opts);

	query(const user_devices &,
	      const mutable_buffer &,
	      opts);

	query(const m::user::id &,
	      const string_view &device_id,
	      const mutable_buffer &,
	      opts);

	query(const m::user::id &,
	      const mutable_buffer &,
	      opts);

	query() = default;
};

struct ircd::m::fed::user::keys::claim
:request
{
	using opts = fed::user::opts;
	using device = std::pair<string_view, string_view>;
	using devices = vector_view<const device>;
	using user_devices = std::pair<m::user::id, devices>;
	using users_devices = vector_view<const user_devices>;
	using users_devices_map = std::map<m::user::id, json::object>;

	static json::object make_content(const mutable_buffer &, const users_devices &);
	static json::object make_content(const mutable_buffer &, const users_devices_map &);

	explicit operator json::object() const
	{
		return json::object
		{
			in.content
		};
	}

	explicit
	claim(const json::object &content,
	      const mutable_buffer &,
	      opts);

	claim(const users_devices_map &,
	      const mutable_buffer &,
	      opts);

	claim(const users_devices &,
	      const mutable_buffer &,
	      opts);

	claim(const user_devices &,
	      const mutable_buffer &,
	      opts);

	claim(const m::user::id &,
	      const device &device,
	      const mutable_buffer &,
	      opts);

	claim(const m::user::id &,
	      const string_view &device_id,
	      const string_view &algorithm,
	      const mutable_buffer &,
	      opts);

	claim() = default;
};
