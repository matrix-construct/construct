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
#define HAVE_IRCD_FS_WAIT_H

namespace ircd::fs
{
	enum class ready :uint8_t;
	struct wait_opts extern const wait_opts_default;

	string_view reflect(const ready &);

	void wait(const fd &, const wait_opts &);
}

enum class ircd::fs::ready
:uint8_t
{
	ANY,       ///< Wait for anything.
	READ,      ///< Ready for read().
	WRITE,     ///< Ready for write().
	ERROR,     ///< Has error.
};

/// Options for a write operation
struct ircd::fs::wait_opts
:opts
{
	enum ready ready;

	wait_opts(const enum ready &ready = ready::ANY);
};

inline
ircd::fs::wait_opts::wait_opts(const enum ready &ready)
:opts{0, op::WAIT}
,ready{ready}
{}
