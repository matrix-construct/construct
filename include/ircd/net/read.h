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
#define HAVE_IRCD_NET_READ_H

namespace ircd::net
{
	// Non-blocking; read into buffers in a single syscall
	size_t read_one(socket &, const vector_view<const mutable_buffer> &);
	size_t read_one(socket &, const mutable_buffer &);

	// Yields until something is read into buffers.
	size_t read_any(socket &, const vector_view<const mutable_buffer> &);
	size_t read_any(socket &, const mutable_buffer &);

	// Yields until buffers are entirely full.
	size_t read_all(socket &, const vector_view<const mutable_buffer> &);
	size_t read_all(socket &, const mutable_buffer &);

	// Alias to read_any();
	size_t read(socket &, const vector_view<const mutable_buffer> &);
	size_t read(socket &, const mutable_buffer &);

	// Yields until len has been discarded
	size_t discard_all(socket &, const size_t &len);
}

/// Alias to read_any();
inline size_t
ircd::net::read(socket &socket,
                const mutable_buffer &buffer)
{
	return read_any(socket, buffer);
}

/// Alias to read_any();
inline size_t
ircd::net::read(socket &socket,
                const vector_view<const mutable_buffer> &buffers)
{
	return read_any(socket, buffers);
}

inline size_t
ircd::net::read_all(socket &socket,
                    const mutable_buffer &buffer)
{
	const mutable_buffer buffers[]
	{
		buffer
	};

	return read_all(socket, buffers);
}

inline size_t
ircd::net::read_any(socket &socket,
                    const mutable_buffer &buffer)
{
	const mutable_buffer buffers[]
	{
		buffer
	};

	return read_any(socket, buffers);
}

inline size_t
ircd::net::read_one(socket &socket,
                    const mutable_buffer &buffer)
{
	const mutable_buffer buffers[]
	{
		buffer
	};

	return read_one(socket, buffers);
}
