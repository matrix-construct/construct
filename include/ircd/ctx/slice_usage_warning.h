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
#define HAVE_IRCD_CTX_SLICE_USAGE_WARNING_H

namespace ircd::ctx {
inline namespace this_ctx
{
	struct slice_usage_warning;
}}

#ifndef NDEBUG
struct ircd::ctx::this_ctx::slice_usage_warning
{
	string_view fmt;
	va_rtti ap;
	ulong start;

	template<class... args>
	slice_usage_warning(const string_view &fmt, args&&... ap)
	:slice_usage_warning{fmt, va_rtti{ap...}}
	{}

	explicit slice_usage_warning(const string_view &fmt, va_rtti &&ap);
	~slice_usage_warning() noexcept;
};
#else
struct ircd::ctx::this_ctx::slice_usage_warning
{
	slice_usage_warning(const string_view &fmt, ...)
	{
		// In at least gcc 6.3.0 20170519, template param packs are not DCE'ed
		// so legacy varargs are used here.
	}
};
#endif
