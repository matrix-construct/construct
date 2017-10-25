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
#define HAVE_IRCD_NET_REMOTE_H

namespace ircd::net
{
	struct remote;
	struct ipport;
	struct hostport;

	const uint16_t &port(const hostport &);
	const uint16_t &port(const ipport &);
	uint16_t &port(hostport &);
	uint16_t &port(ipport &);

	bool is_v6(const ipport &);
	bool is_v4(const ipport &);

	const uint128_t &host6(const ipport &);
	const uint32_t &host4(const ipport &);
	const auto &host(const hostport &);
	uint128_t &host6(ipport &);
	uint32_t &host4(ipport &);
	auto &host(hostport &);

	string_view string(const uint32_t &, const mutable_buffer &buf);
	string_view string(const uint128_t &, const mutable_buffer &buf);
	string_view string(const hostport &, const mutable_buffer &buf);
	string_view string(const ipport &, const mutable_buffer &buf);
	string_view string(const remote &, const mutable_buffer &buf);

	std::string string(const uint32_t &);
	std::string string(const uint128_t &);
	std::string string(const hostport &);
	std::string string(const ipport &);
	std::string string(const remote &);
}

/// This structure holds a hostname and port usually fresh from user input
/// intended for resolution.
///
/// The host can be specified as a hostname or hostname:port string. If no
/// port argument is specified and no port is present in the host string
/// then 8448 is assumed.
///
struct ircd::net::hostport
:std::pair<std::string, uint16_t>
{
	hostport(std::string s, const uint16_t &port = 8448);

	friend std::ostream &operator<<(std::ostream &, const hostport &);
};

/// This lightweight structure holds an IP address and port in native byte
/// form. It is usually returned from the result of a resolution but may also
/// serve as input to a reverse resolution. Either way, this allocation-free
/// structure is useful for storing raw IP/port data, even in large sets.
///
/// Constructing this object with a hostname will yield an IP address
/// resolution, the result being saved in the structure. The hostname argument
/// is not saved in the structure; consider using net::remote instead.
///
/// The TYPE field is only a boolean for now indicating IPv4 or IPv6; the
/// official way to query this is to use net::is_v6(ipport) etc and not
/// std::get the type directly.
///
struct ircd::net::ipport
:std::tuple<std::array<uint8_t, 16>, uint16_t, bool>
{
	enum { IP, PORT, TYPE };

	operator bool() const;
	bool operator!() const             { return !static_cast<bool>(*this);     }

	// Trivial constructors
	ipport(const uint32_t &ip, const uint16_t &port);
	ipport(const uint128_t &ip, const uint16_t &port);

	// DNS lookup! May yield ircd::ctx!
	explicit ipport(const std::string &hostname, const std::string &port);
	explicit ipport(const std::string &hostname, const uint16_t &port);
	ipport(const string_view &hostname, const string_view &port = "8448");
	ipport(const string_view &hostname, const uint16_t &port);
	ipport(const hostport &);
	ipport();

	friend std::ostream &operator<<(std::ostream &, const ipport &);
};

/// This structure combines features of hostport and ipport to hold a remote's
/// resolved IP in bytes, a port number, and an optional hostname string which
/// may have been used to resolve the IP, or may have been resolved from the
/// IP, or may just be empty, but either way still has some use being carried
/// along as part of this struct.
struct ircd::net::remote
:ircd::net::ipport
{
	std::string hostname;

	operator bool() const;
	bool operator!() const             { return !static_cast<bool>(*this);     }

	explicit remote(std::string hostname, const std::string &port = "8448"s);
	remote(std::string hostname, const uint16_t &port);
	remote(const string_view &hostname, const string_view &port = "8448");
	remote(const string_view &hostname, const uint16_t &port);
	explicit remote(const ipport &);
	remote(hostport);
	remote() = default;

	friend std::ostream &operator<<(std::ostream &, const remote &);
};

inline ircd::net::remote::operator
bool()
const
{
	return bool(static_cast<const ipport &>(*this)) || !hostname.empty();
}

inline
ircd::net::ipport::ipport()
{
	std::get<PORT>(*this) = 0;
	std::get<TYPE>(*this) = 0;
	//auto &ip(std::get<IP>(*this));
	//std::fill(begin(ip), end(ip), 0x00);
}

inline
ircd::net::ipport::ipport(const uint32_t &ip,
                          const uint16_t &p)
{
	std::get<TYPE>(*this) = false;
	host4(*this) = ip;
	port(*this) = p;
}

inline
ircd::net::ipport::ipport(const uint128_t &ip,
                          const uint16_t &p)
{
	std::get<TYPE>(*this) = true;
	host6(*this) = ip;
	port(*this) = p;
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

inline auto &
ircd::net::host(hostport &hp)
{
	return hp.first;
}

inline const auto &
ircd::net::host(const hostport &hp)
{
	return hp.first;
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

inline uint16_t &
ircd::net::port(hostport &hp)
{
	return hp.second;
}

inline const uint16_t &
ircd::net::port(const hostport &hp)
{
	return hp.second;
}
