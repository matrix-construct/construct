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
#define HAVE_IRCD_NET_CLOSE_H

namespace ircd::net
{
	enum class dc;
	struct close_opts extern const close_opts_default;
	using close_callback = std::function<void (std::exception_ptr)>;

	string_view reflect(const dc) noexcept;

	// Callback-based closer.
	void close(socket &, const close_opts &, close_callback);
	void close(socket &, const dc &, close_callback);

	// Future-based closer.
	ctx::future<void> close(socket &, const close_opts & = close_opts_default);
	ctx::future<void> close(socket &, const dc &);

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
	static conf::item<milliseconds> default_timeout;

	/// The type of close() to be conducted is specified here.
	net::dc type { dc::SSL_NOTIFY };

	/// The coarse duration allowed for the close() process.
	milliseconds timeout { default_timeout };

	/// If specified, these socket options will be applied when conducting
	/// the disconnect (useful for adding an SO_LINGER time etc).
	const sock_opts *sopts { nullptr };

	/// For portable clean disconnection shutdown(2) might be called prior to
	/// close(2). Setting this option to dc::RST skips the shutdown(2) when
	/// the caller deems it unnecessary. At this time it only affects non-SSL
	/// sockets and in the future we will have io_uring(7) fuse these calls.
	net::dc shutdown { dc::FIN };
};
