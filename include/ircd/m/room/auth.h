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
#define HAVE_IRCD_M_ROOM_AUTH_H

/// Interface to the auth_chain / auth_dag.
///
struct ircd::m::room::auth
{
	struct refs;
	struct chain;
	struct hookdata;
	using types = vector_view<const string_view>;
	using events_view = vector_view<const event *>;
	using passfail = std::tuple<bool, std::exception_ptr>;
	IRCD_M_EXCEPTION(m::error, error, http::INTERNAL_SERVER_ERROR)
	IRCD_M_EXCEPTION(error, AUTH_FAIL, http::UNAUTHORIZED)
	using FAIL = AUTH_FAIL;

	static bool is_power_event(const event &);
	static std::array<event::idx, 5> relative_idx(const event &, const room &);
	static std::array<event::idx, 5> static_idx(const event &);

	static passfail check(const event &, hookdata &);
	static passfail check(const event &, const vector_view<event::idx> &);

	static passfail check_static(const event &);
	static passfail check_present(const event &);
	static passfail check_relative(const event &);
	static void check(const event &);

	static bool generate(json::stack::array &, const m::room &, const m::event &);
	static json::array generate(const mutable_buffer &, const m::room &, const m::event &);
};

/// Interface to the references made by other power events to this power
/// event in the `auth_events`. This interface only deals with power events,
/// it doesn't care if a non-power event referenced a power event. This does
/// not contain the auth-chain or state resolution algorithm here, those are
/// later constructed out of this data.
struct ircd::m::room::auth::refs
{
	event::idx idx;

  public:
	using closure_bool = event::closure_idx_bool;

	bool for_each(const string_view &type, const closure_bool &) const;
	bool for_each(const closure_bool &) const;

	bool has(const string_view &type) const;
	bool has(const event::idx &) const;

	size_t count(const string_view &type) const;
	size_t count() const;

	refs(const event::idx &idx)
	:idx{idx}
	{}
};

struct ircd::m::room::auth::chain
{
	using closure = event::closure_idx_bool;

	event::idx idx;

  public:
	bool for_each(const closure &) const;
	bool has(const string_view &type) const;
	size_t depth() const;

	chain(const event::idx &idx)
	:idx{idx}
	{}
};

class ircd::m::room::auth::hookdata
{
	const event *find(const event::closure_bool &) const;

  public:
	event::prev prev;
	vector_view<const event *> auth_events;
	const event *auth_create {nullptr};
	const event *auth_power {nullptr};
	const event *auth_join_rules {nullptr};
	const event *auth_member_target {nullptr};
	const event *auth_member_sender {nullptr};

	bool allow {false};
	std::exception_ptr fail;

	hookdata(const event &, const events_view &auth_events);
	hookdata() = default;
};
