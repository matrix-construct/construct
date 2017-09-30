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

namespace ircd::net
{
	IRCD_EXCEPTION(ircd::error, error)
	IRCD_EXCEPTION(error, broken_pipe)
	IRCD_EXCEPTION(error, disconnected)

	struct init;
	struct socket;
	struct listener;

	using hostport_pair = std::pair<std::string, uint16_t>;
	using hostport = IRCD_WEAK_T(hostport_pair);

	const auto &host(const hostport &);
	const auto &port(const hostport &);
	std::string string(const hostport &);
	string_view string(const hostport &, const mutable_buffer &buf);

	bool connected(const socket &) noexcept;
	hostport local_hostport(const socket &);
	hostport remote_hostport(const socket &);
	size_t available(const socket &);

	size_t write(socket &, const ilist<const_buffer> &);     // write_all
	size_t write(socket &, const iov<const_buffer> &);       // write_all
	size_t write(socket &, const const_buffer &);            // write_all
	size_t write(socket &, iov<const_buffer> &);             // write_some

	size_t read(socket &, const ilist<mutable_buffer> &);    // read_all
	size_t read(socket &, const iov<mutable_buffer> &);      // read_all
	size_t read(socket &, const mutable_buffer &);           // read_all
	size_t read(socket &, iov<mutable_buffer> &);            // read_some
}

namespace ircd
{
	using net::socket;
	using net::hostport;
	using net::host;
	using net::port;
	using net::string;
}

struct ircd::net::listener
{
	struct acceptor;

	IRCD_EXCEPTION(ircd::error, error)

  private:
	std::unique_ptr<struct acceptor> acceptor;

  public:
	listener(const json::object &options);
	listener(const std::string &options);
	~listener() noexcept;
};

struct ircd::net::init
{
	init();
	~init() noexcept;
};

inline const auto &
ircd::net::port(const hostport &hostport)
{
	return hostport.second;
}

inline const auto &
ircd::net::host(const hostport &hostport)
{
	return hostport.first;
}
