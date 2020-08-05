// Matrix Construct
//
// Copyright (C) Matrix Construct Developers, Authors & Contributors
// Copyright (C) 2016-2018 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

namespace ircd::m
{
	const std::array<http::header, 1> _error_headers
	{{
		{ "Content-Type", "application/json; charset=utf-8" },
	}};
}

thread_local
decltype(ircd::m::error::fmtbuf)
ircd::m::error::fmtbuf
{};

//
// error::error
//

ircd::m::error::error()
:error
{
	internal, http::INTERNAL_SERVER_ERROR, std::string{},
}
{}

ircd::m::error::error(std::string c)
:error
{
	internal, http::INTERNAL_SERVER_ERROR, std::move(c),
}
{}

ircd::m::error::error(const http::code &c)
:error
{
	internal, c, std::string{},
}
{}

ircd::m::error::error(const http::code &c,
                      const json::members &members)
:error
{
	internal, c, json::strung{members},
}
{}

ircd::m::error::error(const http::code &c,
                      const json::iov &iov)
:error
{
	internal, c, json::strung{iov},
}
{}

ircd::m::error::error(const http::code &c,
                      const json::object &object)
:error
{
	internal, c, json::strung{object},
}
{}

ircd::m::error::error(internal_t,
                      const http::code &c,
                      std::string object)
:http::error
{
	c, std::move(object), vector_view<const http::header>{_error_headers}
}
{
	if(!content.empty())
	{
		strlcat(ircd::exception::buf, " ");
		strlcat(ircd::exception::buf, errcode());
		strlcat(ircd::exception::buf, " :");
		strlcat(ircd::exception::buf, errstr());
	}
}

// Out-of-line placement
ircd::m::error::~error()
noexcept
{
}

ircd::string_view
ircd::m::error::errstr()
const noexcept try
{
	const json::object &content
	{
		this->http::error::content
	};

	if(json::type(content, std::nothrow) == json::STRING)
		return string_view{content};

	const json::string &ret
	{
		content["error"]
	};

	return ret;
}
catch(...)
{
	return "(There was an error with the error object)"_sv;
}

ircd::string_view
ircd::m::error::errcode()
const noexcept try
{
	const json::object &content
	{
		this->http::error::content
	};

	if(json::type(content, std::nothrow) == json::STRING)
		return "M_UNKNOWN"_sv;

	const json::string &ret
	{
		content.get("errcode", "M_UNKNOWN"_sv)
	};

	return ret;
}
catch(...)
{
	return "M_BAD_ERROR"_sv;
}
