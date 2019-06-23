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
#define HAVE_IRCD_DEMANGLE_H

namespace ircd {
inline namespace util
{
	IRCD_EXCEPTION(ircd::error, demangle_error)
	IRCD_EXCEPTION(demangle_error, not_mangled)

	string_view demangle(const mutable_buffer &out, const char *const &symbol);
	string_view demangle(const mutable_buffer &out, const string_view &symbol);
	std::string demangle(const string_view &symbol);
	std::string demangle(const char *const &symbol);
	template<class T> string_view demangle(const mutable_buffer &out);
	template<class T> std::string demangle();
}}

template<class T>
std::string
ircd::util::demangle()
{
	return demangle(typeid(T).name());
}

template<class T>
ircd::string_view
ircd::util::demangle(const mutable_buffer &out)
{
	return demangle(out, typeid(T).name());
}
