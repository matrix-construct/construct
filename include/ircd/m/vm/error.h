// The Construct
//
// Copyright (C) The Construct Developers, Authors & Contributors
// Copyright (C) 2016-2020 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

#pragma once
#define HAVE_IRCD_M_VM_ERROR_H

namespace ircd::m::vm
{
	struct error;
}

struct ircd::m::vm::error
:m::error
{
	vm::fault code;

	template<class... args>
	error(const http::code &, const fault &, const string_view &fmt, args&&... a);

	template<class... args>
	error(const fault &, const string_view &fmt, args&&... a);

	template<class... args>
	error(const string_view &fmt, args&&... a);
};

template<class... args>
ircd::m::vm::error::error(const string_view &fmt,
                          args&&... a)
:error
{
	http::INTERNAL_SERVER_ERROR, fault::GENERAL, fmt, std::forward<args>(a)...
}
{}

template<class... args>
ircd::m::vm::error::error(const fault &code,
                          const string_view &fmt,
                          args&&... a)
:error
{
	http_code(code), code, fmt, std::forward<args>(a)...
}
{}

template<class... args>
ircd::m::vm::error::error(const http::code &httpcode,
                          const fault &code,
                          const string_view &fmt,
                          args&&... a)
:m::error
{
	child, httpcode, reflect(code), fmt, std::forward<args>(a)...
}
,code
{
	code
}
{}
