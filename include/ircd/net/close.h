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
#define HAVE_IRCD_NET_CLOSE_H

namespace ircd::net
{
	enum class dc;
	struct close_opts extern const close_opts_default;
	using close_callback = std::function<void (std::exception_ptr)>;

	// Callback-based closer.
	void close(socket &, const close_opts &, close_callback);

	// Future-based closer.
	ctx::future<void> close(socket &, const close_opts & = close_opts_default);

	// Fire-and-forget helper callback for close().
	extern const close_callback close_ignore;
}

/// Types of disconnection. SSL_NOTIFY is the recommended type of
/// disconnection and is usually default. RST is an immediate
/// alternative which has no asynchronous operations.
enum class ircd::net::dc
{
	RST,                ///< hardest immediate termination
	FIN,                ///< sd graceful shutdown both directions
	FIN_SEND,           ///< sd graceful shutdown send side
	FIN_RECV,           ///< sd graceful shutdown recv side
	SSL_NOTIFY,         ///< SSL close_notify
};

/// Close options structure.
struct ircd::net::close_opts
{
	close_opts() = default;
	close_opts(const net::dc &);

	/// The type of close() to be conducted is specified here.
	net::dc type { dc::SSL_NOTIFY };

	/// The coarse duration allowed for the close() process.
	milliseconds timeout { 5000ms };

	/// If specified, these socket options will be applied when conducting
	/// the disconnect (useful for adding an SO_LINGER time etc).
	const sock_opts *sopts { nullptr };
};

/// Allows for implicit construction of close_opts in arguments to close()
/// without requiring brackets for the close_opts& argument.
inline
ircd::net::close_opts::close_opts(const net::dc &type)
:type{type}
{}
