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
#define HAVE_IRCD_MAGICK_H

/// GraphicsMagick Library Interface
namespace ircd::magick
{
	IRCD_EXCEPTION(ircd::error, error)

	struct thumbnail;

	std::tuple<ulong, string_view> version();
}

struct ircd::magick::thumbnail
:const_buffer
{
	thumbnail(const const_buffer &in,
	          const mutable_buffer &out,
	          const std::pair<size_t, size_t> &xy);
};
