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
#define HAVE_IRCD_UTF_H

/// Unicode Transformation Format
namespace ircd::utf
{
	IRCD_EXCEPTION(ircd::error, error)
}

/// Unicode Transformation Format (8-bit)
namespace ircd::utf8
{
	using utf::error;

	template<class u32xN> u32xN length(const u32xN &codepoint) noexcept;
	template<class u32xN> u32xN encode(const u32xN &codepoint) noexcept;
}

/// Unicode Transformation Format (16-bit)
namespace ircd::utf16
{
	using utf::error;

	u32x8 convert_u32x8(const u8x16 &pairs) noexcept;
}
