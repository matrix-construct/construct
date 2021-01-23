// The Construct
//
// Copyright (C) The Construct Developers, Authors & Contributors
// Copyright (C) 2016-2021 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

#include <RB_INC_PNG_H

namespace ircd::png
{
	static void handle_error(png_structp, const char *) noexcept(false);
	static void handle_warn(png_structp, const char *) noexcept;
	static void *handle_alloc(png_structp, size_t) noexcept;
	static void handle_free(png_structp, void *) noexcept;
	static void handle_read(png_structp, uint8_t *, size_t) noexcept;

	extern log::log log;
}

decltype(ircd::png::log)
ircd::png::log
{
	"png"
};

decltype(ircd::png::version_api)
ircd::png::version_api
{
	"png", info::versions::API,
	#ifdef HAVE_PNG_H
	PNG_LIBPNG_VER,
	{
		PNG_LIBPNG_VER_MAJOR,
		PNG_LIBPNG_VER_MINOR,
		PNG_LIBPNG_VER_RELEASE,
	},
	rstrip(lstrip(PNG_HEADER_VERSION_STRING, " "), "\n")
	#endif
};

/// Since libpng may not have loaded prior to static initialization there is
/// no linked library identification. libmagick takes care of loading libpng
/// dynamically if it requires it for us.
decltype(ircd::png::version_abi)
ircd::png::version_abi
{
	"png", info::versions::ABI,
};

bool
ircd::png::is_animated(const const_buffer &buf)
#ifdef PNG_APNG_SUPPORTED
{
	// Cannot be a PNG
	if(unlikely(size(buf) < 8))
		return false;

	png_infop info {nullptr};
	png_structp handle
	{
		png_create_read_struct_2
		(
			PNG_LIBPNG_VER_STRING,
			nullptr,
			&handle_error,
			&handle_warn,
			nullptr,
			&handle_alloc,
			&handle_free
		)
	};

	const unwind destroy
	{
		[&handle, &info]
		{
			png_destroy_read_struct(&handle, &info, nullptr);
		}
	};

	if(unlikely(!handle))
		return false;

	if(unlikely(!(info = png_create_info_struct(handle))))
		return false;

	const_buffer src {buf};
	png_set_read_fn(handle, &src, &handle_read);

	// Invokes the read callback a few times to traverse the buffer.
	png_read_info(handle, info);

	// If there's a non-zero result there's acTL for animation.
	uint32_t num_frames {0}, num_plays {0};
	return png_get_acTL(handle, info, &num_frames, &num_plays) != 0;
}
#else
{
	// If there's no libpng there's no reason to decide APNG's right now
	// because they won't be thumbnailed by magick anyway.
	#warning "Upgrade your libpng version for animation detection."
	return false;
}
#endif

void
ircd::png::handle_read(png_structp handle,
                       uint8_t *const ptr,
                       size_t size)
noexcept
{
	// Destination where libpng wants data
	const mutable_buffer dst
	{
		reinterpret_cast<char *>(ptr), size
	};

	// Get source buffer from our callback user data.
	const_buffer &src
	{
		*reinterpret_cast<const_buffer *>(png_get_io_ptr(handle))
	};

	// Copy from our buffer to libpng buffer.
	const size_t copied
	{
		copy(dst, src)
	};

	// Advance our source pointer for the amount copied.
	const size_t consumed
	{
		consume(src, copied)
	};

	assert(copied == consumed);
}

void *
ircd::png::handle_alloc(png_structp handle,
                        size_t size)
noexcept
{
	return std::malloc(size);
}

void
ircd::png::handle_free(png_structp handle,
                       void *const ptr)
noexcept
{
	std::free(ptr);
}

void
ircd::png::handle_warn(png_structp handle,
                       const char *const msg)
noexcept
{
	log::dwarning
	{
		log, "handle(%p) :%s",
		static_cast<const void *>(handle),
		msg,
	};
}

void
ircd::png::handle_error(png_structp handle,
                        const char *const msg)
noexcept(false)
{
	log::error
	{
		log, "handle(%p) :%s",
		static_cast<const void *>(handle),
		msg,
	};

	throw error
	{
		"%s", msg
	};
}
