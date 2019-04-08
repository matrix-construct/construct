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
#define HAVE_IRCD_FS_OP_H

namespace ircd::fs
{
	enum class op :uint8_t;

	string_view reflect(const op &);
}

namespace ircd::fs::aio
{
	op translate(const int &);
}

/// The enumerated operation code to identify the type of request being
/// made at runtime from any abstract list of requests.
///
enum class ircd::fs::op
:uint8_t
{
	NOOP     = 0,
	READ     = 1,
	WRITE    = 2,
	SYNC     = 3,
	WAIT     = 4,
};
