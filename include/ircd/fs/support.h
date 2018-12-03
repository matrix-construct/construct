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
	// Indicator lights for AIO
	extern const bool aio;             // Any AIO support.
	extern const bool aio_fsync;       // Kernel supports CMD_FSYNC
	extern const bool aio_fdsync;      // Kernel supports CMD_FDSYNC

	// Test if O_DIRECT supported at target path
	bool direct_io(const string_view &path);

	// Test if fallocate() is supported at target path
	bool fallocate(const string_view &path, const write_opts &wopts = write_opts_default);
}
