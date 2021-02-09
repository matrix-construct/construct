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
#define HAVE_IRCD_NET_HOSTPORT_H

namespace ircd::net
{
	struct hostport;

	// abstraction bleed
	extern const uint16_t canon_port;
	extern const string_view canon_service;

	const uint16_t &port(const hostport &);
	const string_view &host(const hostport &);
	const string_view &service(const hostport &);

	uint16_t &port(hostport &);
	string_view &host(hostport &);
	string_view &service(hostport &);

	string_view string(const mutable_buffer &out, const hostport &);
	string_view canonize(const mutable_buffer &out, const hostport &);
	std::string canonize(const hostport &);
	std::ostream &operator<<(std::ostream &, const hostport &);
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
	IRCD_OVERLOAD(verbatim)

	string_view host;
	string_view service;
	uint16_t port {0};

	explicit operator bool() const;
	bool operator!() const;

	hostport(const string_view &host, const string_view &service, const uint16_t &port = 0);
	hostport(const string_view &host, const uint16_t &port);
	hostport(const string_view &amalgam);
	hostport(const string_view &amalgam, verbatim_t);
	explicit hostport(const rfc3986::uri &);
	hostport() = default;
};

/// Creates a host:service pair from a hostname and a service name string.
/// When passed to net::dns() this will indicate SRV resolution. If no
/// SRV record is found.
inline
ircd::net::hostport::hostport(const string_view &host,
                              const string_view &service,
                              const uint16_t &port)
:host
{
	rfc3986::host(host)
}
,service
{
	service
}
,port
{
	port
}
{}

/// Constructs from an rfc3986 URI i.e https://foo.com or https://foo.com:8080
inline
ircd::net::hostport::hostport(const rfc3986::uri &uri)
:host
{
	rfc3986::host(uri.remote)
}
,service
{
	!rfc3986::port(uri.remote)?
		uri.scheme:
		string_view{}
}
,port
{
	rfc3986::port(uri.remote)
}
{}

/// Creates a host:port pair from a hostname and a port number. When
/// passed to net::dns() no SRV resolution will be done because no
/// service name has been given. (unless manually optioned when using
/// net::dns).
inline
ircd::net::hostport::hostport(const string_view &host,
                              const uint16_t &port)
:hostport
{
	host, string_view{}, port
}
{}

inline
ircd::net::hostport::operator
bool()
const
{
	return bool(host);
}

inline bool
ircd::net::hostport::operator!()
const
{
	return !bool(*this);
}

inline ircd::string_view &
ircd::net::service(hostport &hp)
{
	return hp.service;
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
