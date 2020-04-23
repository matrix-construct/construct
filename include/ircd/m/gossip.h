// The Construct
//
// Copyright (C) The Construct Developers, Authors & Contributors
// Copyright (C) 2016-2020 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

#pragma once
#define HAVE_IRCD_M_GOSSIP_H

namespace ircd::m::gossip
{
	struct opts;
	struct gossip;

	extern log::log log;
};

struct ircd::m::gossip::gossip
{
	gossip(const room::id &, const opts &);
};

struct ircd::m::gossip::opts
{
	/// The remote server to gossip with. May be empty to gossip with every
	/// server in the room.
	string_view remote;

	/// An event to gossip about. May be empty to determine which event must
	/// be gossiped about.
	m::event::id event_id;

	/// Coarse timeout for various network interactions.
	milliseconds timeout {7500ms};
};
