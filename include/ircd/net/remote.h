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
bool()
const
{
	return resolved() || !hostname.empty();
}
