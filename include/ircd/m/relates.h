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
#define HAVE_IRCD_M_RELATES_H

namespace ircd::m
{
	struct relates;
	struct relates_to;
}

struct ircd::m::relates_to
:json::tuple
<
	/// Target event_id
	json::property<name::event_id, json::string>,

	/// Relation type
	json::property<name::rel_type, json::string>,

	/// m.in_reply_to object
	json::property<name::m_in_reply_to, json::object>
>
{
	using super_type::tuple;
	using super_type::operator=;
};

/// Interface to the rel_type of the M_RELATES dbs::ref type
///
struct ircd::m::relates
{
	event::refs refs;

  public:
	using closure = util::closure_bool
	<
		std::function,
		const event::idx &, const json::object &, const m::relates_to &
	>;

	bool for_each(const string_view &type, const closure &) const;
	bool for_each(const closure &) const;

	event::idx get(const string_view &type, const uint at = 0) const;
	event::idx latest(const string_view &type, uint *const at = nullptr) const;

	bool has(const string_view &type, const event::idx &) const;
	bool has(const string_view &type) const;
	bool has(const event::idx &) const;

	size_t count(const string_view &type = {}) const;
	bool prefetch(const string_view &type = {}, const bool depth = false) const;

	relates(const event::refs &refs)
	:refs{refs}
	{}
};
