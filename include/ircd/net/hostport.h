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

	const uint16_t &port(const hostport &);
	const string_view &host(const hostport &);
	const string_view &service(const hostport &);

	uint16_t &port(hostport &);
	string_view &host(hostport &);

	string_view string(const mutable_buffer &out, const hostport &);
	string_view canonize(const mutable_buffer &out, const hostport &, const uint16_t &port = 8448);
	std::string canonize(const hostport &, const uint16_t &port = 8448);
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

	hostport(const string_view &host, const string_view &service, const uint16_t &port = 8448);
	hostport(const string_view &host, const uint16_t &port);
	hostport(const string_view &amalgam);
	hostport() = default;

	friend std::ostream &operator<<(std::ostream &, const hostport &);
};

/// Creates a host:service pair from a hostname and a service name string.
/// When passed to net::dns() this will indicate SRV resolution. If no
/// SRV record is found
/// TODO: todo: the servce is translated into its proper port.
/// TODO: now: the port 8448 is used with the hostname.
inline
ircd::net::hostport::hostport(const string_view &host,
                              const string_view &service,
                              const uint16_t &port)
:host{host}
,service{service}
,port{port}
{}

/// Creates a host:port pair from a hostname and a port number. When
/// passed to net::dns() no SRV resolution will be done because no
/// service name has been given. (unless manually optioned when using
/// net::dns).
inline
ircd::net::hostport::hostport(const string_view &host,
                              const uint16_t &port)
:host{host}
,service{}
,port{port}
{}

/// Creates a host:service or host:port pair from the single string literally
/// containing the colon deliminated values. If the suffix is a port number
/// then the behavior for the port number constructor applies; if a service
/// string then the service constructor applies; if empty (just a hostname)
/// then service "matrix" is assumed with port 8448 fallback.
inline
ircd::net::hostport::hostport(const string_view &amalgam)
:host
{
	//TODO: grammar
	rsplit(amalgam, ':').first
}
{
	const auto port
	{
		rsplit(amalgam, ':').second
	};

	if(amalgam == host || empty(port))
		return;

	if(try_lex_cast<uint16_t>(port))
	{
		this->service = {};
		this->port = lex_cast<uint16_t>(port);
	} else {
		this->service = port;
	}
}

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
