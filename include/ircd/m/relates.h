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

	/// m.in_reply_to object
	json::property<name::m_in_reply_to, json::object>,

	/// Relation type
	json::property<name::rel_type, json::string>

>
{
	using super_type::tuple;
	using super_type::operator=;
};

/// Interface to the rel_type of the M_RELATES dbs::ref type
///
struct ircd::m::relates
{
	using closure = util::closure_bool
	<
		std::function,
		const event::idx &, const json::object &, const m::relates_to &
	>;

	event::refs refs;
	bool match_sender {false};
	bool prefetch_depth {false};
	bool prefetch_sender {false};

  private:
	bool _each(const string_view &, const closure &, const event::idx &) const;

  public:
	bool for_each(const string_view &type, const closure &) const;
	bool for_each(const closure &) const;

	bool rfor_each(const string_view &type, const closure &) const;
	bool rfor_each(const closure &) const;

	event::idx get(const string_view &type, const uint at = 0) const;
	event::idx latest(const string_view &type, uint *const at = nullptr) const;

	bool has(const string_view &type, const event::idx &) const;
	bool has(const string_view &type) const;
	bool has(const event::idx &) const;

	size_t count(const string_view &type = {}) const;
	bool prefetch(const string_view &type = {}) const;
};

inline bool
ircd::m::relates::rfor_each(const closure &closure)
const
{
	return rfor_each(string_view{}, closure);
}

inline bool
ircd::m::relates::rfor_each(const string_view &rel_type,
                            const closure &closure)
const
{
	return refs.rfor_each(dbs::ref::M_RELATES, [this, &rel_type, &closure]
	(const auto &event_idx, const auto &type)
	{
		return _each(rel_type, closure, event_idx);
	});
}

inline bool
ircd::m::relates::for_each(const closure &closure)
const
{
	return for_each(string_view{}, closure);
}

inline bool
ircd::m::relates::for_each(const string_view &rel_type,
                           const closure &closure)
const
{
	return refs.for_each(dbs::ref::M_RELATES, [this, &rel_type, &closure]
	(const auto &event_idx, const auto &type)
	{
		return _each(rel_type, closure, event_idx);
	});
}
