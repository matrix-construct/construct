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

	wait_opts(const ready &, const milliseconds &timeout);
	wait_opts(const ready &);
	wait_opts() = default;
};

inline
ircd::net::wait_opts::wait_opts(const ready &type)
:type{type}
{}

inline
ircd::net::wait_opts::wait_opts(const ready &type,
                                const milliseconds &timeout)
:type{type}
,timeout{timeout}
{}
