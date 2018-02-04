// Matrix Construct
//
// Copyright (C) Matrix Construct Developers, Authors & Contributors
// Copyright (C) 2016-2018 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

#pragma once
#define HAVE_IRCD_JS_CALL_H

namespace ircd {
namespace js   {

value
call(const function::handle &func,
     const object::handle &that,
     const vector<value>::handle &args = {});

value
call(const value::handle &val,
     const object::handle &that,
     const vector<value>::handle &args = {});

value
call(const char *const &name,
     const object::handle &that,
     const vector<value>::handle &args = {});

value
call(const std::string &name,
     const object::handle &that,
     const vector<value>::handle &args = {});

inline value
call(const function::handle &func,
     const value::handle &that,
     const vector<value>::handle &args = {})
{
	const object _that(that);
	return call(func, _that, args);
}

} // namespace js
} // namespace ircd
