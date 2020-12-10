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
#define HAVE_IRCD_M_FED_EVENT_AUTH_H

namespace ircd::m::fed
{
	struct event_auth;
};

struct ircd::m::fed::event_auth
:request
{
	struct opts;

	explicit operator json::array() const
	{
		const json::object object
		{
			in.content
		};

		return object["auth_chain"];
	}

	event_auth(const m::room::id &,
	           const m::event::id &,
	           const mutable_buffer &,
	           opts);

	event_auth() = default;
};

struct ircd::m::fed::event_auth::opts
:request::opts
{
	/// Receive fast auth_chain_ids from construct; or auth_chain from synapse.
	bool ids {false};

	/// Receive slower auth_chain_ids; supported by all servers.
	bool ids_only {false};
};
