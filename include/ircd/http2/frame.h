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
#define HAVE_IRCD_HTTP2_FRAME_H

namespace ircd::http2
{
	struct frame;
}

struct ircd::http2::frame
{
	struct header;
	struct settings;
	enum type :uint8_t;

	static string_view reflect(const type &);
};

struct ircd::http2::frame::header
{
	uint32_t len        : 24;
	enum type type;
	uint8_t flags;
	uint32_t            : 1;
	uint32_t stream_id  : 31;
}
__attribute__((packed));

enum ircd::http2::frame::type
:uint8_t
{
	DATA           = 0x0,
	HEADERS        = 0x1,
	PRIORITY       = 0x2,
	RST_STREAM     = 0x3,
	SETTINGS       = 0x4,
	PUSH_PROMISE   = 0x5,
	PING           = 0x6,
	GOAWAY         = 0x7,
	WINDOW_UPDATE  = 0x8,
	CONTINUATION   = 0x9,
};
