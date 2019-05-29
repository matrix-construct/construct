// Matrix Construct
//
// Copyright (C) Matrix Construct Developers, Authors & Contributors
// Copyright (C) 2016-2018 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

#include <RB_INC_MAGICK_API_H

namespace ircd::magick
{
	static void init();
	static void fini();
}

ircd::mapi::header
IRCD_MODULE
{
	"GraphicsMagick Library support for media manipulation",
	ircd::magick::init,
	ircd::magick::fini
};

//
// init
//

void
ircd::magick::init()
{
	ulong lib_version(0);
	const char *const lib_version_text
	{
		::GetMagickVersion(&lib_version)
	};

	log::debug
	{
		"Initializing Magick Library include:%lu [%s] library:%lu [%s]",
		ulong(MagickLibVersion),
		MagickLibVersionText,
		lib_version,
		lib_version_text,
	};

	if(lib_version != ulong(MagickLibVersion))
		log::warning
		{
			"Magick Library version mismatch headers:%lu library:%lu",
			ulong(MagickLibVersion),
			lib_version,
		};

	::InitializeMagick(nullptr);
}

void
ircd::magick::fini()
{
	log::debug
	{
		"Shutting down Magick Library..."
	};

	::DestroyMagick();
}
