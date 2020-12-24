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
#define HAVE_IRCD_NET_READ_H

namespace ircd::net
{
	using mutable_buffers = vector_view<const mutable_buffer>;

	// Observers
	size_t readable(const socket &);
	size_t available(const socket &) noexcept;

	// Non-blocking; read into buffers in a single syscall
	size_t read_one(socket &, const mutable_buffers &);
	size_t read_one(socket &, const mutable_buffer &);

	// Non-blocking; read as much as possible into buffers
	size_t read_any(socket &, const mutable_buffers &);
	size_t read_any(socket &, const mutable_buffer &);

	// Yields until something is read into buffers.
	size_t read_few(socket &, const mutable_buffers &);
	size_t read_few(socket &, const mutable_buffer &);

	// Yields until buffers are entirely full.
	size_t read_all(socket &, const mutable_buffers &);
	size_t read_all(socket &, const mutable_buffer &);

	// Alias to read_few();
	size_t read(socket &, const mutable_buffers &);
	size_t read(socket &, const mutable_buffer &);

	// Non-blocking; discard up to len, but less may be discarded.
	size_t discard_any(socket &, const size_t &len);

	// Yields until len has been discarded
	size_t discard_all(socket &, const size_t &len);
}

/// Alias to read_few();
inline size_t
ircd::net::read(socket &socket,
                const mutable_buffer &buffer)
{
	return read_few(socket, buffer);
}

/// Alias to read_few();
inline size_t
ircd::net::read(socket &socket,
                const mutable_buffers &buffers)
{
	return read_few(socket, buffers);
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
ircd::net::read_few(socket &socket,
                    const mutable_buffer &buffer)
{
	const mutable_buffer buffers[]
	{
		buffer
	};

	return read_few(socket, buffers);
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
