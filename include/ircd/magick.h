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

	struct crop;
	struct shave;
	struct scale;
	struct thumbnail;
	struct thumbcrop;

	std::tuple<ulong, string_view> version();
}

/// Composite thumbnailer to resize close to the requested dimension but
/// preserving original aspect ratio; then crop to requested dimension.
struct ircd::magick::thumbcrop
{
	using dimensions = std::pair<size_t, size_t>; // x, y
	using result_closure = std::function<void (const const_buffer &)>;

	thumbcrop(const const_buffer &in,
	          const dimensions &,
	          const result_closure &);
};

/// Fast thumbnailer
struct ircd::magick::thumbnail
{
	using dimensions = std::pair<size_t, size_t>; // x, y
	using result_closure = std::function<void (const const_buffer &)>;

	thumbnail(const const_buffer &in,
	          const dimensions &,
	          const result_closure &);
};

/// Basic resize (library selected algorithm)
struct ircd::magick::scale
{
	using dimensions = std::pair<size_t, size_t>; // x, y
	using result_closure = std::function<void (const const_buffer &)>;

	scale(const const_buffer &in,
	      const dimensions &,
	      const result_closure &);
};

/// Shave off sides (center-crop to dimensions)
struct ircd::magick::shave
{
	using offset = std::pair<ssize_t, ssize_t>; // x, y
	using dimensions = std::pair<size_t, size_t>; // x, y
	using result_closure = std::function<void (const const_buffer &)>;

	shave(const const_buffer &in,
	      const dimensions &,
	      const offset &,
	      const result_closure &);
};

/// Crop to dimensions at offset
struct ircd::magick::crop
{
	using offset = std::pair<ssize_t, ssize_t>; // x, y
	using dimensions = std::pair<size_t, size_t>; // x, y
	using result_closure = std::function<void (const const_buffer &)>;

	crop(const const_buffer &in,
	     const dimensions &,
	     const offset &,
	     const result_closure &);
};
