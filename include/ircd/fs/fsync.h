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
#define HAVE_IRCD_FS_FSYNC_H

namespace ircd::fs
{
	struct fsync_opts extern const fsync_opts_default;

	void fdsync(const fd &, const fsync_opts & = fsync_opts_default);
	void fsync(const fd &, const fsync_opts & = fsync_opts_default);
}

/// Options for a write operation
struct ircd::fs::fsync_opts
{
	fsync_opts() = default;

	/// Request priority (this option may be improved, avoid for now)
	int16_t priority {0};
};
