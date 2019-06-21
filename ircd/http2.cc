// Matrix Construct
//
// Copyright (C) Matrix Construct Developers, Authors & Contributors
// Copyright (C) 2016-2019 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

decltype(ircd::http2::connection_preface)
ircd::http2::connection_preface
{
	"PRI * HTTP/2.0\r\n\r\nSM\r\n\r\n"
};

///////////////////////////////////////////////////////////////////////////////
//
// stream.h
//

ircd::http2::stream::stream()
:state
{
	state::IDLE
}
{
}

ircd::string_view
ircd::http2::reflect(const enum stream::state &state)
{
	switch(state)
	{
		case stream::state::IDLE:                  return "IDLE";
		case stream::state::RESERVED_LOCAL:        return "RESERVED_LOCAL";
		case stream::state::RESERVED_REMOTE:       return "RESERVED_REMOTE";
		case stream::state::OPEN:                  return "OPEN";
		case stream::state::HALF_CLOSED_LOCAL:     return "HALF_CLOSED_LOCAL";
		case stream::state::HALF_CLOSED_REMOTE:    return "HALF_CLOSED_REMOTE";
		case stream::state::CLOSED:                return "CLOSED";
	}

	return "??????";
}

///////////////////////////////////////////////////////////////////////////////
//
// settings.h
//

/// RFC 7540 6.5.2 default settings
///
ircd::http2::settings::settings()
:array_type
{
	4096,    // HEADER_TABLE_SIZE
	1,       // ENABLE_PUSH
	0,       // MAX_CONCURRENT_STREAMS (unlimited)
	65535,   // INITIAL_WINDOW_SIZE
	16384,   // MAX_FRAME_SIZE
	0,       // MAX_HEADER_LIST_SIZE (unlimited)
}
{
}

ircd::string_view
ircd::http2::reflect(const frame::settings::code &code)
{
	switch(code)
	{
		case frame::settings::code::HEADER_TABLE_SIZE:        return "HEADER_TABLE_SIZE";
		case frame::settings::code::ENABLE_PUSH:              return "ENABLE_PUSH";
		case frame::settings::code::MAX_CONCURRENT_STREAMS:   return "MAX_CONCURRENT_STREAMS";
		case frame::settings::code::INITIAL_WINDOW_SIZE:      return "INITIAL_WINDOW_SIZE";
		case frame::settings::code::MAX_FRAME_SIZE:           return "MAX_FRAME_SIZE";
		case frame::settings::code::MAX_HEADER_LIST_SIZE:     return "MAX_HEADER_LIST_SIZE";
		case frame::settings::code::_NUM_:                    assert(0);
	}

	return "??????";
}

///////////////////////////////////////////////////////////////////////////////
//
// frame.h
//

static_assert
(
    sizeof(ircd::http2::frame::header) == 9
);


///////////////////////////////////////////////////////////////////////////////
//
// error.h
//

namespace ircd::http2
{
	thread_local char error_fmt_buf[512];
}

ircd::http2::error::error()
:error
{
	code::INTERNAL_ERROR
}
{
}

ircd::http2::error::error(const enum code &code)
:ircd::error
{
	"(%x) %s",
	uint32_t(code),
	reflect(code),
}
,code
{
	code
}
{
}

ircd::http2::error::error(const enum code &code,
                          const string_view &fmt,
                          va_rtti &&ap)
:ircd::error
{
	"(%x) %s :%s",
	uint32_t(code),
	reflect(code),
	string_view{fmt::vsprintf
	{
		error_fmt_buf, fmt, std::move(ap)
	}}
}
,code
{
	code
}
{
}

ircd::string_view
ircd::http2::reflect(const enum error::code &code)
{
	switch(code)
	{
		case error::code::NO_ERROR:               return "NO_ERROR";
		case error::code::PROTOCOL_ERROR:         return "PROTOCOL_ERROR";
		case error::code::INTERNAL_ERROR:         return "INTERNAL_ERROR";
		case error::code::FLOW_CONTROL_ERROR:     return "FLOW_CONTROL_ERROR";
		case error::code::SETTINGS_TIMEOUT:       return "SETTINGS_TIMEOUT";
		case error::code::STREAM_CLOSED:          return "STREAM_CLOSED";
		case error::code::FRAME_SIZE_ERROR:       return "FRAME_SIZE_ERROR";
		case error::code::REFUSED_STREAM:         return "REFUSED_STREAM";
		case error::code::CANCEL:                 return "CANCEL";
		case error::code::COMPRESSION_ERROR:      return "COMPRESSION_ERROR";
		case error::code::CONNECT_ERROR:          return "CONNECT_ERROR";
		case error::code::ENHANCE_YOUR_CALM:      return "ENHANCE_YOUR_CALM";
		case error::code::INADEQUATE_SECURITY:    return "INADEQUATE_SECURITY";
		case error::code::HTTP_1_1_REQUIRED:      return "HTTP_1_1_REQUIRED";
	}

	return "??????";
}
