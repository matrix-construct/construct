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
#define HAVE_IRCD_NET_WAIT_H

namespace ircd::net
{
	enum class ready :uint8_t;
	struct wait_opts extern const wait_opts_default;
	using wait_callback_ec = std::function<void (const error_code &)>;
	using wait_callback_eptr = std::function<void (std::exception_ptr)>;

	string_view reflect(const ready &);

	// Asynchronous callback when ready with error_code
	void wait(socket &, const wait_opts &, wait_callback_ec);

	// Asynchronous callback when ready with exception_ptr (convenience)
	void wait(socket &, const wait_opts &, wait_callback_eptr);

	// Yields ircd::ctx for the wait condition.
	void wait(socket &, const wait_opts & = wait_opts_default);

	// Yields ircd::ctx for the wait condition. Won't throw the error_code.
	error_code wait(nothrow_t, socket &, const wait_opts & = wait_opts_default);

	// Explicit overload to return a ctx::future
	ctx::future<void> wait(use_future_t, socket &, const wait_opts & = wait_opts_default);
}

/// Types of things to wait for the socket to have "ready"
enum class ircd::net::ready
:uint8_t
{
	ANY,       ///< Wait for anything.
	READ,      ///< Data is available for a read().
	WRITE,     ///< Space is free in the SNDBUF for a write().
	ERROR,     ///< Socket has an error.
};

struct ircd::net::wait_opts
{
	ready type
	{
		ready::ANY
	};

	milliseconds timeout
	{
		-1
	};

	wait_opts(const ready &, const milliseconds &timeout) noexcept;
	wait_opts(const ready &) noexcept;
	wait_opts() = default;
};

inline
ircd::net::wait_opts::wait_opts(const ready &type)
noexcept
:type{type}
{}

inline
ircd::net::wait_opts::wait_opts(const ready &type,
                                const milliseconds &timeout)
noexcept
:type{type}
,timeout{timeout}
{}
