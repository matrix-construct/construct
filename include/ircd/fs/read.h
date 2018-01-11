// Copyright (C) Matrix Construct Developers, Authors & Contributors
// Copyright (C) 2016-2018 Jason Volk
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies.
//
// THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
// IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
// WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
// DISCLAIMED. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT,
// INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
// (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
// SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
// HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
// STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
// IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
// POSSIBILITY OF SUCH DAMAGE.

#pragma once
#define HAVE_IRCD_FS_READ_H

namespace ircd::fs
{
	struct read_opts extern const read_opts_default;
	using read_callback = std::function<void (std::exception_ptr, const string_view &)>;

	// Asynchronous callback-based read into buffer; callback views read portion.
	void read(const string_view &path, const mutable_raw_buffer &, const read_opts &, read_callback);
	void read(const string_view &path, const mutable_raw_buffer &, read_callback);

	// Yields ircd::ctx for read into buffer; returns view of read portion.
	string_view read(const string_view &path, const mutable_raw_buffer &, const read_opts & = read_opts_default);

	// Yields ircd::ctx for read into allocated string; returns that string
	std::string read(const string_view &path, const read_opts & = read_opts_default);
}

/// Options for a read operation
struct ircd::fs::read_opts
{
	read_opts() = default;
	read_opts(const off_t &);

	/// Offset in the file to start the read from.
	off_t offset {0};

	/// Request priority (this option may be improved, avoid for now)
	int16_t priority {0};
};

inline
ircd::fs::read_opts::read_opts(const off_t &offset)
:offset{offset}
{}

inline void
ircd::fs::read(const string_view &path,
               const mutable_raw_buffer &buf,
               read_callback callback)
{
	return read(path, buf, read_opts_default, std::move(callback));
}
