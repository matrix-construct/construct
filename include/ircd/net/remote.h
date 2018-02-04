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
#define HAVE_IRCD_NET_REMOTE_H

namespace ircd::net
{
	struct remote;

	string_view string(const mutable_buffer &out, const remote &);
}

namespace ircd
{
	using net::remote;
}

/// This structure combines features of hostport and ipport to hold a remote's
/// resolved IP in bytes, a port number, and an optional hostname string which
/// may have been used to resolve the IP, or may have been resolved from the
/// IP, or may be used for certificate Common Name verification, or may just
/// be empty, but anyway still has some use in most cases being carried along.
///
struct ircd::net::remote
:ircd::net::ipport
{
	std::string hostname;

	explicit operator bool() const;
	operator hostport() const;
	bool operator!() const             { return !static_cast<bool>(*this);     }
	bool resolved() const;

	explicit remote(const ipport &ipp)
	:ipport{ipp}
	{}

	explicit remote(const hostport &hostport)
	:ipport{uint32_t(0), net::port(hostport)}
	,hostname{net::host(hostport)}
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
hostport()
const
{
	return { hostname, port(*this) };
}

inline ircd::net::remote::operator
bool()
const
{
	return resolved() || !hostname.empty();
}
