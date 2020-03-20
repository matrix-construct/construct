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
#define HAVE_IRCD_FS_SUPPORT_H

namespace ircd::fs::support
{
	// Runtime-gauged support indicators
	extern const bool pwritev2;
	extern const bool preadv2;
	extern const bool append;
	extern const bool nowait;
	extern const bool hipri;
	extern const bool sync;
	extern const bool dsync;
	extern const bool rwh_write_life;
	extern const bool rwf_write_life;
	extern const bool aio;
	extern const bool aio_fsync;
	extern const bool aio_fdsync;

	// Test if O_DIRECT supported at target path
	bool direct_io(const string_view &path);

	// Test if fallocate() is supported at target path
	bool fallocate(const string_view &path, const write_opts &wopts = write_opts_default);

	size_t rlimit_nofile();    // Get the limit for number of opened files
	size_t rlimit_fsize();     // Get the limit for a file's size

	// Dump information to infolog
	void dump_info();
}
