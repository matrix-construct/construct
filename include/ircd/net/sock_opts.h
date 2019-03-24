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
#define HAVE_IRCD_NET_SOCK_OPTS_H

namespace ircd::net
{
	struct sock_opts;

	bool v6only(const socket &);
	bool blocking(const socket &);
	bool nodelay(const socket &);
	bool keepalive(const socket &);
	time_t linger(const socket &);
	size_t read_bufsz(const socket &);
	size_t write_bufsz(const socket &);
	size_t read_lowat(const socket &);
	size_t write_lowat(const socket &);

	void v6only(socket &, const bool &);
	void blocking(socket &, const bool &);
	void nodelay(socket &, const bool &);
	void keepalive(socket &, const bool &);
	void linger(socket &, const time_t &); // -1 is OFF; >= 0 is ON
	void read_bufsz(socket &, const size_t &bytes);
	void write_bufsz(socket &, const size_t &bytes);
	void read_lowat(socket &, const size_t &bytes);
	void write_lowat(socket &, const size_t &bytes);

	void set(socket &, const sock_opts &);
}

/// Socket options convenience aggregate. This structure allows observation
/// or manipulation of socket options all together. Pass an active socket to
/// the constructor to observe all options. Use net::set(socket, sock_opts) to
/// set all non-ignored options.
struct ircd::net::sock_opts
{
	/// Magic value to not set this option on a set() pass.
	static constexpr int8_t IGN { std::numeric_limits<int8_t>::min() };

	int8_t v6only { IGN };
	int8_t blocking { IGN };             // Simulates blocking behavior
	int8_t nodelay { IGN };
	int8_t keepalive { IGN };
	time_t linger { IGN };               // -1 is OFF; >= 0 is ON
	ssize_t read_bufsz { IGN };
	ssize_t write_bufsz { IGN };
	ssize_t read_lowat { IGN };
	ssize_t write_lowat { IGN };

	sock_opts(const socket &);          // Get options from socket
	sock_opts() = default;
};
