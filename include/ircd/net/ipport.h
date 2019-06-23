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
#define HAVE_IRCD_NET_IPPORT_H

namespace ircd::net
{
	struct ipport;

	const uint128_t &host6(const ipport &);
	const uint32_t &host4(const ipport &);
	uint128_t &host6(ipport &);
	uint32_t &host4(ipport &);

	const uint16_t &port(const ipport &);
	uint16_t &port(ipport &);

	bool is_loop(const ipport &);
	bool is_v6(const ipport &);
	bool is_v4(const ipport &);

	string_view string(const mutable_buffer &out, const ipport &);
	std::ostream &operator<<(std::ostream &, const ipport &);
}

namespace ircd
{
	using net::ipport;
	using net::host;
	using net::port;
}

/// This lightweight structure holds an IP address and port in native byte
/// form. It is usually returned from the result of a resolution but may also
/// serve as input to a reverse resolution. Either way, this allocation-free
/// structure is useful for storing raw IP/port data, even in large sets.
///
struct ircd::net::ipport
:std::pair<ipaddr, uint16_t>
{
	enum { IP, PORT };

	struct cmp;
	struct cmp_ip;
	struct cmp_port;

	explicit operator bool() const;
	bool operator!() const             { return !static_cast<bool>(*this);     }

	operator const ipaddr &() const;
	operator ipaddr &();

	template<class iparg> ipport(iparg&&, const uint16_t &port);
	template<class iparg> ipport(iparg&&, const string_view &port);
	ipport(const string_view &amalgam);
	ipport();
};

struct ircd::net::ipport::cmp_port
{
	bool operator()(const ipport &a, const ipport &b) const;
};

struct ircd::net::ipport::cmp_ip
{
	bool operator()(const ipport &a, const ipport &b) const;
};

struct ircd::net::ipport::cmp
:std::less<ipport>
{
	using std::less<ipport>::less;
};

inline
ircd::net::ipport::ipport()
:std::pair<ipaddr, uint16_t>
{
	{}, 0
}
{}

inline
ircd::net::ipport::ipport(const string_view &amalgam)
:std::pair<ipaddr, uint16_t>
{
	rfc3986::host(amalgam), rfc3986::port(amalgam)
}
{}

template<class iparg>
ircd::net::ipport::ipport(iparg&& arg,
                          const string_view &port)
:ipport
{
	std::forward<iparg>(arg), lex_cast<uint16_t>(port)
}
{}

template<class iparg>
ircd::net::ipport::ipport(iparg&& arg,
                          const uint16_t &port)
:std::pair<ipaddr, uint16_t>
{
	std::forward<iparg>(arg), port
}
{}

inline ircd::net::ipport::operator
ipaddr &()
{
	return this->first;
}

inline ircd::net::ipport::operator
const ipaddr &()
const
{
	return this->first;
}

inline ircd::net::ipport::operator
bool()
const
{
	return host6(*this) != uint128_t(0);
}

inline ircd::uint128_t &
ircd::net::host6(ipport &ipp)
{
	return host6(std::get<ipport::IP>(ipp));
}

inline const ircd::uint128_t &
ircd::net::host6(const ipport &ipp)
{
	return host6(std::get<ipport::IP>(ipp));
}

inline bool
ircd::net::is_v6(const ipport &ipp)
{
	return is_v6(std::get<ipport::IP>(ipp));
}

inline uint32_t &
ircd::net::host4(ipport &ipp)
{
	return host4(std::get<ipport::IP>(ipp));
}

inline const uint32_t &
ircd::net::host4(const ipport &ipp)
{
	return host4(std::get<ipport::IP>(ipp));
}

inline bool
ircd::net::is_v4(const ipport &ipp)
{
	return is_v4(std::get<ipport::IP>(ipp));
}

inline uint16_t &
ircd::net::port(ipport &ipp)
{
	return std::get<ipport::PORT>(ipp);
}

inline const uint16_t &
ircd::net::port(const ipport &ipp)
{
	return std::get<ipport::PORT>(ipp);
}

inline bool
ircd::net::is_loop(const ipport &ipp)
{
	return is_loop(std::get<ipport::IP>(ipp));
}
