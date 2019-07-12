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
#define HAVE_IRCD_STRL_H

// Vintage
namespace ircd
{
	struct strlcpy;
	struct strlcat;
}

/// This is a function. It works the same as the standard strlcpy() but it has
/// some useful modernizations and may be informally referred to as strlcpy++.
///
/// - It optionally works with string_view inputs and ircd::buffer outputs.
/// This allows for implicit size parameters and increases its safety while
/// simplifying its usage (no more sizeof(buf) where buf coderots into char*).
///
/// - Its objectification allows for a configurable return type. The old
/// strlcpy() returned a size integer type. When using string_view's and
/// buffers this would generally lead to the pattern { dst, strlcpy(dst, src) }
/// and this is no longer necessary.
///
struct ircd::strlcpy
{
	mutable_buffer ret;

  public:
	operator string_view() const
	{
		return ret;
	}

	operator size_t() const
	{
		return size(ret);
	}

	strlcpy(char *const &dst, const string_view &src, const size_t &max)
	:ret{[&]() -> mutable_buffer
	{
		if(!max)
			return {};

		const auto len(std::min(src.size(), max - 1));
		buffer::copy(mutable_buffer(dst, len), src);
		dst[len] = '\0';
		return { dst, len };
	}()}
	{}

	#ifndef HAVE_STRLCPY
	strlcpy(char *const &dst, const char *const &src, const size_t &max)
	:strlcpy{dst, string_view{src, strnlen(src, max)}, max}
	{}
	#endif

	strlcpy(const mutable_buffer &dst, const string_view &src)
	:strlcpy{data(dst), src, size(dst)}
	{}
};

/// This is a function. It works the same as the standard strlcat() but it has
/// some useful modernizations and may be informally referred to as strlcat++.
/// see: ircd::strlcpy().
///
struct ircd::strlcat
{
	mutable_buffer ret;

  public:
	operator string_view() const
	{
		return ret;
	}

	operator size_t() const
	{
		return size(ret);
	}

	strlcat(char *const &dst, const string_view &src, const size_t &max)
	:ret{[&]() -> mutable_buffer
	{
		const auto pos{strnlen(dst, max)};
		const auto remain{max - pos};
		strlcpy(dst + pos, src, remain);
		return { dst, pos + src.size() };
	}()}
	{}

	#ifndef HAVE_STRLCAT
	strlcat(char *const &dst, const char *const &src, const size_t &max)
	:strlcat{dst, string_view{src, ::strnlen(src, max)}, max}
	{}
	#endif

	strlcat(const mutable_buffer &dst, const string_view &src)
	:strlcat{data(dst), src, size(dst)}
	{}
};
