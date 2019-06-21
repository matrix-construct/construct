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
#define HAVE_IRCD_HTTP2_ERROR_H

namespace ircd::http2
{
	struct error;
}

struct ircd::http2::error
:ircd::error
{
	enum code :uint32_t;

	enum code code;

	error();
	error(const enum code &);

	explicit
	error(const enum code &,
	      const string_view &fmt,
	      va_rtti &&ap);

	template<class... args>
	error(const enum code &,
	      const string_view &fmt,
	      args&&...);

	template<class... args>
	error(const string_view &fmt,
	      args&&...);
};

namespace ircd::http2
{
	string_view reflect(const enum error::code &);
}

enum ircd::http2::error::code
:uint32_t
{
	NO_ERROR                  = 0x0,
	PROTOCOL_ERROR            = 0x1,
	INTERNAL_ERROR            = 0x2,
	FLOW_CONTROL_ERROR        = 0x3,
	SETTINGS_TIMEOUT          = 0x4,
	STREAM_CLOSED             = 0x5,
	FRAME_SIZE_ERROR          = 0x6,
	REFUSED_STREAM            = 0x7,
	CANCEL                    = 0x8,
	COMPRESSION_ERROR         = 0x9,
	CONNECT_ERROR             = 0xa,
	ENHANCE_YOUR_CALM         = 0xb,
	INADEQUATE_SECURITY       = 0xc,
	HTTP_1_1_REQUIRED         = 0xd,
};

template<class... args>
ircd::http2::error::error(const string_view &fmt,
                          args&&... a)
:error
{
	code::INTERNAL_ERROR, fmt, va_rtti{std::forward<args>(a)...}
}
{}

template<class... args>
ircd::http2::error::error(const enum code &code,
                          const string_view &fmt,
                          args&&... a)
:error
{
	code, fmt, va_rtti{std::forward<args>(a)...}
}
{}
