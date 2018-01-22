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
#define HAVE_IRCD_NET_OPEN_H

namespace ircd::net
{
	struct open_opts;
	using open_callback = std::function<void (std::exception_ptr)>;

	// Open existing socket with callback.
	void open(socket &, const open_opts &, open_callback);

	// Open new socket with callback.
	std::shared_ptr<socket> open(const open_opts &, open_callback);

	// Open new socket with future.
	ctx::future<std::shared_ptr<socket>> open(const open_opts &);
}

/// Connection options structure. This is provided when making a client
/// connection with a socket. Unless otherwise noted it usually has to
/// remain in scope as a const reference for the duration of that process.
/// Some of its members are also thin and will have to remain in scope along
/// with it.
struct ircd::net::open_opts
{
	// Get the proper target CN from the options structure
	friend string_view common_name(const open_opts &);

	open_opts() = default;
	explicit open_opts(const net::ipport &ipport);
	open_opts(const net::hostport &hostport);
	open_opts(const net::remote &remote);

	/// Remote's hostname and port. This will be used for address resolution
	/// if an ipport is not also provided later. The hostname will also be used
	/// for certificate /CN verification if common_name is not provided later.
	net::hostport hostport;

	/// Remote's resolved IP and port. Providing this skips DNS resolution if
	/// hostport is not given; required if so.
	net::ipport ipport;

	/// The duration allowed for the TCP connection.
	milliseconds connect_timeout { 5000ms };

	/// Pointer to a sock_opts structure which will be applied to this socket
	/// if given. Defaults to null; no application is made.
	const sock_opts *sopts { nullptr };

	/// Option to toggle whether to perform the SSL handshake; you want true.
	bool handshake { true };

	/// The duration allowed for the SSL handshake
	milliseconds handshake_timeout { 5000ms };

	/// Option to toggle whether to perform any certificate verification; if
	/// false, everything no matter what is considered valid; you want true.
	bool verify_certificate { true };

	/// Option to toggle whether to perform CN verification to ensure the
	/// certificate is signed to the actual host we want to talk to. When
	/// true, see the comments for `common_name`. Otherwise if false, any
	/// common_name will pass muster.
	bool verify_common_name { true };

	/// Option to toggle whether to perform CN verification for self-signed
	/// certificates. This is set to false for compatibility purposes as many
	/// self-signed certificates have either no CN or CN=localhost and none
	/// of that really matters anyway.
	bool verify_self_signed_common_name { false };

	/// The expected /CN of the target. This should be the remote's hostname,
	/// If it is empty then `hostport.host` is used. If the signed /CN has
	/// some rfc2818/rfc2459 wildcard we will properly match that for you.
	string_view common_name;

	/// Option to toggle whether to allow self-signed certificates. This
	/// currently defaults to true to not break Matrix development but will
	/// likely change later and require setting to true for specific conns.
	bool allow_self_signed { true };

	/// Option to toggle whether to allow self-signed certificate authorities
	/// in the chain. This is what corporate network nanny's may use to spy.
	bool allow_self_chain { false };
};

/// Constructor intended to provide implicit conversions (no-brackets required)
/// in arguments to open(); i.e open(hostport) rather than open({hostport});
inline
ircd::net::open_opts::open_opts(const net::hostport &hostport)
:hostport{hostport}
{}

/// Constructor intended to provide implicit conversions (no-brackets required)
/// in arguments to open(); i.e open(ipport) rather than open({ipport});
inline
ircd::net::open_opts::open_opts(const net::ipport &ipport)
:ipport{ipport}
{}

inline
ircd::net::open_opts::open_opts(const net::remote &remote)
:hostport{remote.hostname}
,ipport{remote}
{}

inline ircd::string_view
ircd::net::common_name(const open_opts &opts)
{
	return opts.common_name?: opts.hostport.host;
}
