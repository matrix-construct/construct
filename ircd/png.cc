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

/// Since libpng may not loaded prior to static initialization there is
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
	return false;
}
#else
{
	#warning "Upgrade your libpng version for animation detection."
	return false;
}
#endif
