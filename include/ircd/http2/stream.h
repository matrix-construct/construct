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
#define HAVE_IRCD_HTTP2_STREAM_H

namespace ircd::http2
{
	struct stream;
}

struct ircd::http2::stream
{
	enum class state :uint8_t;

	enum state state;

	stream();
};

namespace ircd::http2
{
	string_view reflect(const enum stream::state &);
}

enum class ircd::http2::stream::state
:uint8_t
{
	IDLE,
	RESERVED_LOCAL,
	RESERVED_REMOTE,
	OPEN,
	HALF_CLOSED_LOCAL,
	HALF_CLOSED_REMOTE,
	CLOSED,
};
