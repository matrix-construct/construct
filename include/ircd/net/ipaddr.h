// Matrix Construct
//
// Copyright (C) Matrix Construct Developers, Authors & Contributors
// Copyright (C) 2016-2019 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

#pragma once
#define HAVE_IRCD_NET_IPADDR_H

// Forward declarations for boost because it is not included here.
namespace boost::asio::ip
{
	struct address;
	struct address_v4;
	struct address_v6;
};

namespace ircd::net
{
	union ipaddr;

	const uint128_t &host6(const ipaddr &);
	const uint32_t &host4(const ipaddr &);
	uint128_t &host6(ipaddr &);
	uint32_t &host4(ipaddr &);

	bool is_loop(const ipaddr &);
	bool is_v6(const ipaddr &);
	bool is_v4(const ipaddr &);

	bool operator!(const ipaddr &);
	bool operator<(const ipaddr &, const ipaddr &);
	bool operator==(const ipaddr &, const ipaddr &);

	string_view string_ip4(const mutable_buffer &out, const uint32_t &);
	string_view string_ip6(const mutable_buffer &out, const uint128_t &);
	string_view string(const mutable_buffer &out, const ipaddr &);
	std::ostream &operator<<(std::ostream &, const ipaddr &);

	boost::asio::ip::address_v6 make_address(const uint128_t &);
	boost::asio::ip::address_v4 make_address(const uint32_t &);
	boost::asio::ip::address make_address(const ipaddr &);
	boost::asio::ip::address make_address(const string_view &ip);
}

union ircd::net::ipaddr
{
	struct cmp;

	uint32_t v4;
	uint128_t v6 {0};
	std::array<uint8_t, 16> byte;

	explicit operator bool() const;

	ipaddr(const uint32_t &ip);
	ipaddr(const uint128_t &ip);
	ipaddr(const rfc1035::record::A &);
	ipaddr(const rfc1035::record::AAAA &);
	ipaddr(const boost::asio::ip::address &);
	ipaddr(const string_view &ip);
	ipaddr() = default;
};

static_assert(std::alignment_of<ircd::net::ipaddr>() >= 16);

struct ircd::net::ipaddr::cmp
{
	bool operator()(const ipaddr &a, const ipaddr &b) const;
};

inline ircd::net::ipaddr::operator
bool() const
{
	return bool(v6);
}

inline bool
ircd::net::operator==(const ipaddr &a, const ipaddr &b)
{
	return a.v6 == b.v6;
}

inline bool
ircd::net::operator<(const ipaddr &a, const ipaddr &b)
{
	return a.v6 < b.v6;
}

inline bool
ircd::net::operator!(const ipaddr &a)
{
	return !a.v6;
}

inline uint32_t &
ircd::net::host4(ipaddr &ipaddr)
{
	return ipaddr.v4;
}

inline ircd::uint128_t &
ircd::net::host6(ipaddr &ipaddr)
{
	return ipaddr.v6;
}

inline const uint32_t &
ircd::net::host4(const ipaddr &ipaddr)
{
	return ipaddr.v4;
}

inline const ircd::uint128_t &
ircd::net::host6(const ipaddr &ipaddr)
{
	return ipaddr.v6;
}
