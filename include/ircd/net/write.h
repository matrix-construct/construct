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
#define HAVE_IRCD_NET_WRITE_H

namespace ircd::net
{
	// Non-blocking; writes at most one system-determined amount of
	// bytes or less with at most a single syscall.
	size_t write_one(socket &, const vector_view<const const_buffer> &);
	size_t write_one(socket &, const const_buffer &);

	// Non-blocking; writes as much as possible until the socket buffer
	// is full. This composes multiple write_one()'s.
	size_t write_any(socket &, const vector_view<const const_buffer> &);
	size_t write_any(socket &, const const_buffer &);

	// Blocking; Yields your ircd::ctx until all bytes have been written;
	// advise one uses a timeout in conjunction to prevent DoS.
	size_t write_all(socket &, const vector_view<const const_buffer> &);
	size_t write_all(socket &, const const_buffer &);

	// Alias to write_all();
	size_t write(socket &, const vector_view<const const_buffer> &);
	size_t write(socket &, const const_buffer &);

	// Toggles nodelay to force a transmission.
	void flush(socket &);
}

/// Alias to write_all();
inline size_t
ircd::net::write(socket &socket,
                 const const_buffer &buffer)
{
	return write_all(socket, buffer);
}

/// Alias to write_all();
inline size_t
ircd::net::write(socket &socket,
                 const vector_view<const const_buffer> &buffers)
{
	return write_all(socket, buffers);
}

inline size_t
ircd::net::write_all(socket &socket,
                     const const_buffer &buffer)
{
	const const_buffer buffers[]
	{
		buffer
	};

	return write_all(socket, buffers);
}

inline size_t
ircd::net::write_any(socket &socket,
                     const const_buffer &buffer)
{
	const const_buffer buffers[]
	{
		buffer
	};

	return write_all(socket, buffers);
}

inline size_t
ircd::net::write_one(socket &socket,
                     const const_buffer &buffer)
{
	const const_buffer buffers[]
	{
		buffer
	};

	return write_one(socket, buffers);
}
