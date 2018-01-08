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
#define HAVE_IRCD_NET_HOSTPORT_H

namespace ircd::net
{
	struct hostport;

	const auto &host(const hostport &);
	auto &host(hostport &);

	string_view string(const mutable_buffer &out, const hostport &);
}

namespace ircd
{
	using net::hostport;
	using net::host;
}

/// This structure holds a hostname and port usually fresh from user input
/// intended for resolution.
///
/// The host can be specified as a hostname or hostname:port string. If no
/// port argument is specified and no port is present in the host string
/// then 8448 is assumed.
///
struct ircd::net::hostport
{
	string_view host {"0.0.0.0"};
	string_view port {"8448"};
	uint16_t portnum {0};

	hostport(const string_view &host, const string_view &port)
	:host{host}
	,port{port}
	{}

	hostport(const string_view &host, const uint16_t &portnum)
	:host{host}
	,port{}
	,portnum{portnum}
	{}

	hostport(const string_view &amalgam)
	:host{rsplit(amalgam, ':').first}
	,port{amalgam != host? rsplit(amalgam, ':').second : "8448"}
	{}

	hostport() = default;

	friend std::ostream &operator<<(std::ostream &, const hostport &);
};

inline auto &
ircd::net::host(hostport &hp)
{
	return hp.host;
}

inline const auto &
ircd::net::host(const hostport &hp)
{
	return hp.host;
}
