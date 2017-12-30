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

// Forward declarations for boost because it is not included here.
namespace boost::asio::ip
{
	struct address;
};

namespace ircd::net
{
	struct hostport;
	struct resolve;
	struct ipport;
	struct remote;

	const uint16_t &port(const ipport &);
	uint16_t &port(ipport &);

	bool is_v6(const ipport &);
	bool is_v4(const ipport &);

	const uint128_t &host6(const ipport &);
	const uint32_t &host4(const ipport &);
	const auto &host(const hostport &);
	uint128_t &host6(ipport &);
	uint32_t &host4(ipport &);
	auto &host(hostport &);

	string_view string(const mutable_buffer &out, const uint32_t &);
	string_view string(const mutable_buffer &out, const uint128_t &);
	string_view string(const mutable_buffer &out, const hostport &);
	string_view string(const mutable_buffer &out, const ipport &);
	string_view string(const mutable_buffer &out, const remote &);
}

namespace ircd
{
	using net::hostport;
	using net::ipport;
	using net::remote;
	using net::host;
	using net::port;
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
	,port{rsplit(amalgam, ':').second}
	{}

	hostport() = default;

	friend std::ostream &operator<<(std::ostream &, const hostport &);
};

/// DNS resolution suite.
///
/// There are plenty of ways to resolve plenty of things. Still more to come.
struct ircd::net::resolve
{
	using callback_one = std::function<void (std::exception_ptr, const ipport &)>;
	using callback_many = std::function<void (std::exception_ptr, vector_view<ipport>)>;
	using callback_reverse = std::function<void (std::exception_ptr, std::string)>;

	resolve(const hostport &, callback_one);
	resolve(const hostport &, callback_many);
	resolve(const ipport &, callback_reverse);

	resolve(const hostport &, ctx::future<ipport> &);
	resolve(const hostport &, ctx::future<std::vector<ipport>> &);

	resolve(const vector_view<hostport> &in, const vector_view<ipport> &out);
	resolve(const vector_view<ipport> &in, const vector_view<std::string> &out);
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

	ipport(const hostport &);          // DNS lookup! May yield ircd::ctx!
	ipport(const uint32_t &ip, const uint16_t &port);
	ipport(const uint128_t &ip, const uint16_t &port);
	ipport(const boost::asio::ip::address &, const uint16_t &port);
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
	bool resolved() const;

	explicit remote(const ipport &ipp)
	:ipport{ipp}
	{}

	remote(const hostport &hp)
	:ipport{hp}
	,hostname{std::string{hp.host}}
	{}

	remote() = default;

	friend std::ostream &operator<<(std::ostream &, const remote &);
};

inline bool
ircd::net::remote::resolved()
const
{
	return bool(static_cast<const ipport &>(*this));
}

inline ircd::net::remote::operator
bool()
const
{
	return resolved() || !hostname.empty();
}

inline
ircd::net::ipport::ipport()
{
	std::get<PORT>(*this) = 0;
	std::get<TYPE>(*this) = 0;
	auto &ip(std::get<IP>(*this));
	std::fill(begin(ip), end(ip), 0x0);
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
	return hp.host;
}

inline const auto &
ircd::net::host(const hostport &hp)
{
	return hp.host;
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
