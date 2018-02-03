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

	const uint16_t &port(const hostport &);
	const string_view &host(const hostport &);
	const string_view &service(const hostport &);

	uint16_t &port(hostport &);
	string_view &host(hostport &);

	string_view string(const mutable_buffer &out, const hostport &);
}

namespace ircd
{
	using net::hostport;
	using net::host;
}

/// This structure holds a hostname and service or port usually fresh from
/// user input intended for resolution.
///
/// The host can be passed to the constructor as a hostname or hostname:port
/// string.
///
/// The service field is a string which will trigger an SRV query during
/// resolution and/or be used to figure out the port number. If it is omitted
/// then the numerical port should be specified directly. If it exists then
/// the numerical port will only be used as a fallback, and an SRV response
/// with an alternative port may even override both the host and given
/// numerical port here.
///
struct ircd::net::hostport
{
	string_view host {"0.0.0.0"};
	string_view service {"matrix"};
	uint16_t port {8448};

	bool operator!() const;

	hostport(const string_view &host, const string_view &service);
	hostport(const string_view &host, const uint16_t &port);
	hostport(const string_view &amalgam);
	hostport() = default;

	friend std::ostream &operator<<(std::ostream &, const hostport &);
};

inline
ircd::net::hostport::hostport(const string_view &host,
                              const string_view &service)
:host{host}
,service{service}
{}

inline
ircd::net::hostport::hostport(const string_view &host,
                              const uint16_t &port)
:host{host}
,service{}
,port{port}
{}

inline
ircd::net::hostport::hostport(const string_view &amalgam)
:host
{
	rsplit(amalgam, ':').first
}
,service{}
,port
{
	amalgam != host? lex_cast<uint16_t>(rsplit(amalgam, ':').second) : uint16_t(0)
}
{}

inline bool
ircd::net::hostport::operator!()
const
{
	static const hostport defaults{};
	return net::host(*this) == net::host(defaults);
}

inline ircd::string_view &
ircd::net::host(hostport &hp)
{
	return hp.host;
}

inline uint16_t &
ircd::net::port(hostport &hp)
{
	return hp.port;
}

inline const ircd::string_view &
ircd::net::service(const hostport &hp)
{
	return hp.service;
}

inline const ircd::string_view &
ircd::net::host(const hostport &hp)
{
	return hp.host;
}

inline const uint16_t &
ircd::net::port(const hostport &hp)
{
	return hp.port;
}
