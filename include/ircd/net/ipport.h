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

// Forward declarations for boost because it is not included here.
namespace boost::asio::ip
{
	struct address;
};

namespace ircd::net
{
	struct ipport;

	const uint16_t &port(const ipport &);
	uint16_t &port(ipport &);

	bool is_v6(const ipport &);
	bool is_v4(const ipport &);

	const uint128_t &host6(const ipport &);
	const uint32_t &host4(const ipport &);
	uint128_t &host6(ipport &);
	uint32_t &host4(ipport &);

	string_view string(const mutable_buffer &out, const uint32_t &);
	string_view string(const mutable_buffer &out, const uint128_t &);
	string_view string(const mutable_buffer &out, const ipport &);
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
/// The TYPE field is only a boolean for now indicating IPv4 or IPv6; the
/// official way to query this is to use net::is_v6(ipport) etc and not
/// std::get the type directly.
///
struct ircd::net::ipport
:std::tuple<std::array<uint8_t, 16>, uint16_t, bool>
{
	enum { IP, PORT, TYPE };

	explicit operator bool() const;
	bool operator!() const             { return !static_cast<bool>(*this);     }

	ipport(const uint32_t &ip, const uint16_t &port);
	ipport(const uint128_t &ip, const uint16_t &port);
	ipport(const rfc1035::record::A &, const uint16_t &port);
	ipport(const rfc1035::record::AAAA &, const uint16_t &port);
	ipport(const boost::asio::ip::address &, const uint16_t &port);
	ipport(const string_view &ip, const uint16_t &port);
	ipport(const string_view &ip, const string_view &port);
	ipport();

	friend std::ostream &operator<<(std::ostream &, const ipport &);
};

inline
ircd::net::ipport::ipport()
{
	std::get<PORT>(*this) = 0;
	std::get<TYPE>(*this) = 0;
	auto &ip(std::get<IP>(*this));
	ip.fill(0);
}

inline ircd::net::ipport::operator
bool()
const
{
	return std::get<PORT>(*this) != 0;
}

inline ircd::uint128_t &
ircd::net::host6(ipport &ipp)
{
	auto &bytes{std::get<ipp.IP>(ipp)};
	return *reinterpret_cast<uint128_t *>(bytes.data());
}

inline const ircd::uint128_t &
ircd::net::host6(const ipport &ipp)
{
	const auto &bytes{std::get<ipp.IP>(ipp)};
	return *reinterpret_cast<const uint128_t *>(bytes.data());
}

inline bool
ircd::net::is_v6(const ipport &ipp)
{
	return std::get<ipp.TYPE>(ipp) == true;
}

inline uint32_t &
ircd::net::host4(ipport &ipp)
{
	auto &bytes{std::get<ipp.IP>(ipp)};
	return *reinterpret_cast<uint32_t *>(bytes.data());
}

inline const uint32_t &
ircd::net::host4(const ipport &ipp)
{
	const auto &bytes{std::get<ipp.IP>(ipp)};
	return *reinterpret_cast<const uint32_t *>(bytes.data());
}

inline bool
ircd::net::is_v4(const ipport &ipp)
{
	return std::get<ipp.TYPE>(ipp) == false;
}

inline uint16_t &
ircd::net::port(ipport &ipp)
{
	return std::get<ipp.PORT>(ipp);
}

inline const uint16_t &
ircd::net::port(const ipport &ipp)
{
	return std::get<ipp.PORT>(ipp);
}
