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
#define HAVE_IRCD_NET_H

/// Network IO subsystem.
///
/// Some parts of this system are not automatically included here when they
/// involve types which cannot be forward declared without boost headers.
/// This should not concern most developers as we have wrapped (or you should
/// wrap!) anything we need to expose to the rest of the project, or low-level
/// access may be had by including the asio.h header.
///
namespace ircd::net
{
	struct init;
	struct socket;

	IRCD_EXCEPTION(ircd::error, error)
	IRCD_EXCEPTION(error, disconnected)
	IRCD_EXCEPTION(error, inauthentic)
	IRCD_EXCEPTION(error, not_found)

	// SNOMASK 'N' "net"
	extern log::log log;
}

#include "hostport.h"
#include "ipport.h"
#include "dns.h"
#include "listener.h"
#include "sock_opts.h"
#include "open.h"
#include "close.h"
#include "wait.h"
#include "read.h"
#include "write.h"
#include "scope_timeout.h"

namespace ircd::net
{
	const uint64_t &id(const socket &);
	bool opened(const socket &) noexcept;
	size_t readable(const socket &);
	size_t available(const socket &) noexcept;
	ipport local_ipport(const socket &) noexcept;
	ipport remote_ipport(const socket &) noexcept;
	std::pair<size_t, size_t> bytes(const socket &) noexcept; // <in, out>
	std::pair<size_t, size_t> calls(const socket &) noexcept; // <in, out>

	const_buffer peer_cert_der(const mutable_buffer &, const socket &);
	const_buffer peer_cert_der_sha256(const mutable_buffer &, const socket &);
	string_view peer_cert_der_sha256_b64(const mutable_buffer &, const socket &);
}

// Exports to ircd::
namespace ircd
{
	using net::socket;
}

struct ircd::net::init
{
	init();
	~init() noexcept;
};
