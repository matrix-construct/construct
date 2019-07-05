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

	struct job;
	struct crop;
	struct shave;
	struct scale;
	struct thumbnail;
	struct thumbcrop;

	extern const info::versions version_api, version_abi;
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

struct ircd::magick::job
{
	struct state;

	static thread_local struct job cur, tot;  // current job, total for all jobs
	static thread_local struct state state;   // internal state

	uint64_t id {0};           // monotonic
	int64_t tick {0};          // quantum
	uint64_t ticks {0};        // span
	uint64_t cycles {0};       // rdtsc reference
	uint64_t yields {0};       // ircd::ctx relinquish count for large jobs
	uint64_t intrs {0};        // ircd::ctx interrupt count
	uint64_t errors {0};       // exception/error count
	string_view description;   // only valid for current job duration
	std::exception_ptr eptr;   // apropos exception reference
};
