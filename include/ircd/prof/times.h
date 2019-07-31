// Matrix Construct
//
// Copyright (C) Matrix Construct Developers, Authors & Contributors
// Copyright (C) 2016-2019 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

#pragma once
#define HAVE_IRCD_PROF_TIMES_H

namespace ircd::prof
{
	struct times;
}

/// Frontend to times(2). This has low resolution in practice, but it's
/// very cheap as far as syscalls go; x-platform implementation courtesy
/// of boost::chrono.
struct ircd::prof::times
{
	uint64_t real {0};
	uint64_t kern {0};
	uint64_t user {0};

	times(sample_t);
	times() = default;
};
