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
#define HAVE_IRCD_SERVER_H

/// The interface for when IRCd plays the role of client to other peers
///
namespace ircd::server
{
	struct init;
	struct link;
	struct peer;
	struct request;
	struct tag;

	IRCD_EXCEPTION(ircd::error, error);
	IRCD_EXCEPTION(error, buffer_overrun);
	IRCD_EXCEPTION(error, unavailable);
	IRCD_EXCEPTION(error, canceled);

	using error_code = boost::system::error_code;

	extern ircd::log::log log;
	extern std::map<string_view, std::shared_ptr<peer>> peers;

	size_t tag_count();
	size_t link_count();
	size_t peer_count();

	string_view errmsg(const net::hostport &);
	bool exists(const net::hostport &);
	peer &find(const net::hostport &);
	peer &get(const net::hostport &);
}

#include "tag.h"
#include "request.h"
#include "link.h"
#include "peer.h"

/// Subsystem initialization / destruction from ircd::main
///
struct ircd::server::init
{
	void interrupt();

	init();
	~init() noexcept;
};
