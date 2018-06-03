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
#define HAVE_IRCD_FS_STDIN_H

namespace ircd::fs::stdin
{
	struct tty;

	// Yields ircd::ctx to read line from stdin
	string_view readline(const mutable_buffer &);
}

/// Directly represents the controlling tty of stdin if supported. The primary
/// purpose here is to allow writing text to stdin to provide readline-esque
/// history to the terminal console.
///
/// The member write() must be used, not fs::write(). The latter will throw
/// an error when used on this.
///
struct ircd::fs::stdin::tty
:fd
{
	size_t write(const string_view &);

	tty();
};
