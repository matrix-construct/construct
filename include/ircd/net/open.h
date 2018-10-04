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
/// connection with a socket. The structure itself is copied when passed
/// to open() but for any members that are string_views or pointers they
/// will have to remain valid for the duration of the open().
struct ircd::net::open_opts
{
	static conf::item<milliseconds> default_connect_timeout;
	static conf::item<milliseconds> default_handshake_timeout;
	static conf::item<bool> default_verify_certificate;
	static conf::item<bool> default_allow_self_signed;
	static conf::item<bool> default_allow_self_chain;
	static conf::item<bool> default_allow_expired;

	// Get the proper target CN from the options structure
	friend string_view common_name(const open_opts &);

	open_opts() = default;
	explicit open_opts(const net::ipport &ipport, const net::hostport & = {});
	open_opts(const net::hostport &hostport);

	/// Remote's hostname and port. This will be used for address resolution
	/// if an ipport is not also provided later. The hostname will also be used
	/// for certificate /CN verification if common_name is not provided later.
	net::hostport hostport;

	/// Remote's resolved IP and port. Providing this skips DNS resolution if
	/// hostport is not given; required if so.
	net::ipport ipport;

	/// The duration allowed for the TCP connection.
	milliseconds connect_timeout { default_connect_timeout };

	/// Pointer to a sock_opts structure which will be applied to this socket
	/// if given. Defaults to null; no application is made.
	const sock_opts *sopts { nullptr };

	/// Option to toggle whether to perform the SSL handshake; you want true.
	bool handshake { true };

	/// The duration allowed for the SSL handshake
	milliseconds handshake_timeout { default_handshake_timeout };

	/// Option to toggle whether to perform any certificate verification; if
	/// false, everything no matter what is considered valid; you want true.
	bool verify_certificate { default_verify_certificate };

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
	bool allow_self_signed { default_allow_self_signed };

	/// Option to toggle whether to allow self-signed certificate authorities
	/// in the chain. This is what corporate network nanny's may use to spy.
	bool allow_self_chain { default_allow_self_chain };

	/// Option to allow expired certificates.
	bool allow_expired { default_allow_expired };
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
ircd::net::open_opts::open_opts(const net::ipport &ipport,
                                const net::hostport &hostport)
:hostport{hostport}
,ipport{ipport}
{}

inline ircd::string_view
ircd::net::common_name(const open_opts &opts)
{
	return opts.common_name?: opts.hostport.host;
}
