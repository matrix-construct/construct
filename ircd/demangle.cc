// Matrix Construct
//
// Copyright (C) Matrix Construct Developers, Authors & Contributors
// Copyright (C) 2016-2018 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

#include <cxxabi.h>

thread_local char
outbuf[8192];

std::string
ircd::util::demangle(const char *const &symbol)
{
	const string_view demangled
	{
		demangle(outbuf, symbol)
	};

	return std::string(demangled);
}

std::string
ircd::util::demangle(const string_view &symbol)
{
	const string_view demangled
	{
		demangle(outbuf, symbol)
	};

	return std::string(demangled);
}

ircd::string_view
ircd::util::demangle(const mutable_buffer &out,
                     const string_view &symbol_)
{
	assert(size(symbol_) < 4096);
	thread_local char symbuf[8192];
	const string_view symbol
	{
		symbuf, strlcpy(symbuf, symbol_)
	};

	return demangle(out, symbol.data());
}

ircd::string_view
ircd::util::demangle(const mutable_buffer &out,
                     const char *const &symbol)
{
	int status(0);
	size_t len(size(out));
	const char *const ret
	{
		abi::__cxa_demangle(symbol, data(out), &len, &status)
	};

	switch(status)
	{
		case 0: return string_view
		{
			ret, strnlen(ret, len)
		};

		case -1: throw demangle_error
		{
			"Demangle failed -1: memory allocation failure"
		};

		case -2: throw not_mangled
		{
			"Demangle failed -2: mangled name '%s' is not valid", symbol
		};

		case -3: throw demangle_error
		{
			"Demangle failed -3: invalid argument"
		};

		default: throw demangle_error
		{
			"Demangle failed %d: unknown error", status
		};
	}
}
