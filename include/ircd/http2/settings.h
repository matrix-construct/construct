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
#define HAVE_IRCD_HTTP2_SETTINGS_H

namespace ircd::http2
{
	struct settings;
}

struct ircd::http2::frame::settings
{
	enum code :uint16_t;
	enum flag :decltype(frame::header::flags);
	struct param;

	vector_view<const struct param> param;
};

struct ircd::http2::frame::settings::param
{
	uint16_t id {0};
	uint32_t value {0};
}
__attribute__((packed));

namespace ircd::http2
{
	string_view reflect(const frame::settings::code &);
}

enum ircd::http2::frame::settings::code
:uint16_t
{
	HEADER_TABLE_SIZE         = 0x1,
	ENABLE_PUSH               = 0x2,
	MAX_CONCURRENT_STREAMS    = 0x3,
	INITIAL_WINDOW_SIZE       = 0x4,
	MAX_FRAME_SIZE            = 0x5,
	MAX_HEADER_LIST_SIZE      = 0x6,

	_NUM_
};

enum ircd::http2::frame::settings::flag
:std::underlying_type<ircd::http2::frame::settings::flag>::type
{
	ACK  = (1 << 0),
};

struct ircd::http2::settings
:std::array<uint32_t, num_of<frame::settings::code>()>
{
	using code = frame::settings::code;
	using array_type = std::array<uint32_t, num_of<code>()>;

	settings();
};
