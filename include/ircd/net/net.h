/*
 * Copyright (C) 2017 Charybdis Development Team
 * Copyright (C) 2017 Jason Volk <jason@zemos.net>
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice is present in all copies.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

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
	IRCD_EXCEPTION(ircd::error, error)
	IRCD_EXCEPTION(error, invalid_argument)
	IRCD_EXCEPTION(error, nxdomain)
	IRCD_EXCEPTION(error, broken_pipe)
	IRCD_EXCEPTION(error, disconnected)

	struct init;
	struct remote;
	struct socket;
	struct listener;
	enum class dc;

	// SNOMASK 'N' "net"
	extern struct log::log log;
}

#include "remote.h"
#include "resolve.h"
#include "listener.h"

enum class ircd::net::dc
{
	RST,                ///< hardest immediate termination
	FIN,                ///< sd graceful shutdown both directions
	FIN_SEND,           ///< sd graceful shutdown send side
	FIN_RECV,           ///< sd graceful shutdown recv side
	SSL_NOTIFY,         ///< SSL close_notify (async, errors ignored)
	SSL_NOTIFY_YIELD,   ///< SSL close_notify (yields context, throws)
};

// Public interface to socket.h because it is not included here.
namespace ircd::net
{
	bool connected(const socket &) noexcept;
	size_t available(const socket &) noexcept;
	ipport local_ipport(const socket &) noexcept;
	ipport remote_ipport(const socket &) noexcept;

	const_raw_buffer peer_cert_der(const mutable_raw_buffer &, const socket &);

	size_t write(socket &, const ilist<const_buffer> &);     // write_all
	size_t write(socket &, const iov<const_buffer> &);       // write_all
	size_t write(socket &, iov<const_buffer> &);             // write_some

	size_t read(socket &, const ilist<mutable_buffer> &);    // read_all
	size_t read(socket &, const iov<mutable_buffer> &);      // read_all
	size_t read(socket &, iov<mutable_buffer> &);            // read_some

	std::shared_ptr<socket> connect(const remote &, const milliseconds &timeout = 30000ms);
	bool disconnect(socket &, const dc &type = dc::SSL_NOTIFY) noexcept;

	ctx::future<std::shared_ptr<socket>> open(const ipport &, std::string cn, const milliseconds &timeout = 30000ms);
	ctx::future<std::shared_ptr<socket>> open(const hostport &, const milliseconds &timeout = 30000ms);
}

struct ircd::net::init
{
	init();
	~init() noexcept;
};

namespace ircd
{
	using net::socket;
}
