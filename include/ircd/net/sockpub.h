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
#define HAVE_IRCD_NET_SOCKPUB_H

// This is the public interface to net::socket because socket.h is not part of
// the standard include group as it directly involves boost headers. For
// direct access you may include <ircd/asio.h> in your definition file if
// absolutely necessary.
//
// Any operation on the socket can trigger a pending error (i.e disconnection
// userspace doesn't know about yet) and thus make any call after related to
// the socket invalid and throw. We use noexcept here when there is a
// reasonable default value returned instead of throwing. The goal there is to
// reduce the number of places where the stack can blow up: for example, a
// debug log call that prints the bytes available for reading. During testing
// that may be the place of first observation where an exception keeps getting
// thrown but during release that call won't be there and thus lays the
// foundation for surprise heisenbugs.

namespace ircd::net
{
	struct socket;
	struct sockopts;
	struct connopts;
	enum class dc;
}

namespace ircd
{
	using net::socket;
}

namespace ircd::net
{
	bool connected(const socket &) noexcept;
	size_t readable(const socket &);
	size_t available(const socket &) noexcept;
	ipport local_ipport(const socket &) noexcept;
	ipport remote_ipport(const socket &) noexcept;
	const_raw_buffer peer_cert_der(const mutable_raw_buffer &, const socket &);

	size_t read(socket &, const ilist<mutable_buffer> &);    // read_all
	size_t read(socket &, const iov<mutable_buffer> &);      // read_all
	size_t read(socket &, iov<mutable_buffer> &);            // read_some

	size_t write(socket &, const ilist<const_buffer> &);     // write_all
	size_t write(socket &, const iov<const_buffer> &);       // write_all
	size_t write(socket &, iov<const_buffer> &);             // write_some
	void flush(socket &);

	bool disconnect(socket &, const dc &type) noexcept;
	bool disconnect(socket &) noexcept;

	void open(socket &, const connopts &, std::function<void (std::exception_ptr)>);
	ctx::future<std::shared_ptr<socket>> open(const connopts &);
}

/// Connection options structure. This is provided when making a client
/// connection with a socket. Unless otherwise noted it usually has to
/// remain in scope as a const reference for the duration of that process.
/// Some of its members are also thin and will have to remain in scope along
/// with it.
struct ircd::net::connopts
{
	/// Remote's hostname and port. This will be used for address resolution
	/// if an ipport is not also provided later. The hostname will also be used
	/// for certificate /CN verification if common_name is not provided later.
	net::hostport hostport;

	/// Remote's resolved IP and port. Providing this skips DNS resolution if
	/// hostport is not given; required if so.
	net::ipport ipport;

	/// The duration allowed for the TCP connection.
	milliseconds connect_timeout { 8000ms };

	/// Pointer to a sockopts structure which will be applied to this socket
	/// if given. Defaults to null; no application is made.
	const sockopts *sopts { nullptr };

	/// Option to toggle whether to perform the SSL handshake; you want true.
	bool handshake { true };

	/// The duration allowed for the SSL handshake
	milliseconds handshake_timeout { 8000ms };

	/// Option to toggle whether to perform any certificate verification; if
	/// false, everything no matter what is considered valid; you want true.
	bool verify_certificate { true };

	/// Option to toggle whether to perform CN verification to ensure the
	/// certificate is signed to the actual host we want to talk to. When
	/// true, see the comments for `common_name`. Otherwise if false, any
	/// common_name will pass muster.
	bool verify_common_name { true };

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

// Socket options section
namespace ircd::net
{
	bool blocking(const socket &);
	bool nodelay(const socket &);
	bool keepalive(const socket &);
	time_t linger(const socket &);
	size_t read_bufsz(const socket &);
	size_t write_bufsz(const socket &);
	size_t read_lowat(const socket &);
	size_t write_lowat(const socket &);

	void blocking(socket &, const bool &);
	void nodelay(socket &, const bool &);
	void keepalive(socket &, const bool &);
	void linger(socket &, const time_t &); // -1 is OFF; >= 0 is ON
	void read_bufsz(socket &, const size_t &bytes);
	void write_bufsz(socket &, const size_t &bytes);
	void read_lowat(socket &, const size_t &bytes);
	void write_lowat(socket &, const size_t &bytes);

	void set(socket &, const sockopts &);
}

/// Socket options convenience aggregate. This structure allows observation
/// or manipulation of socket options all together. Pass an active socket to
/// the constructor to observe all options. Use net::set(socket, sockopts) to
/// set all non-ignored options.
struct ircd::net::sockopts
{
	/// Magic value to not set this option on a set() pass.
	static constexpr int8_t IGN { std::numeric_limits<int8_t>::min() };

	int8_t blocking = IGN;             // Simulates blocking behavior
	int8_t nodelay = IGN;
	int8_t keepalive = IGN;
	time_t linger = IGN;               // -1 is OFF; >= 0 is ON
	ssize_t read_bufsz = IGN;
	ssize_t write_bufsz = IGN;
	ssize_t read_lowat = IGN;
	ssize_t write_lowat = IGN;

	sockopts(const socket &);          // Get options from socket
	sockopts() = default;
};

/// Arguments for disconnecting.
enum class ircd::net::dc
{
	RST,                ///< hardest immediate termination
	FIN,                ///< sd graceful shutdown both directions
	FIN_SEND,           ///< sd graceful shutdown send side
	FIN_RECV,           ///< sd graceful shutdown recv side
	SSL_NOTIFY,         ///< SSL close_notify (async, errors ignored)
	SSL_NOTIFY_YIELD,   ///< SSL close_notify (yields context, throws)
};
