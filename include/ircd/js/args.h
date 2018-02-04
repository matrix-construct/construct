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
#define HAVE_IRCD_JS_ARGS_H

namespace ircd {
namespace js   {

struct args
:JS::CallArgs
{
	operator vector<value>::handle() const       { return { *this };                               }

	bool empty() const                           { return length() == 0;                           }
	size_t size() const                          { return length();                                }
	bool has(const size_t &at) const             { return size() > at;                             }

	value at(const size_t &at) const;
	value operator[](const size_t &at) const;

	args(const unsigned &argc, JS::Value *const &argv);
};

inline
args::args(const unsigned &argc,
           JS::Value *const &argv)
:JS::CallArgs{JS::CallArgsFromVp(argc, argv)}
{
}

inline value
args::operator[](const size_t &at)
const
{
	return value{length() > at? JS::CallArgs::operator[](at) : JS::UndefinedValue()};
}

inline value
args::at(const size_t &at)
const
{
	if(unlikely(length() <= at))
		throw range_error("Missing required argument #%zu", at);

	return JS::CallArgs::operator[](at);
}

} // namespace js
} // namespace ircd
