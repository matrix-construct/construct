// The Construct
//
// Copyright (C) The Construct Developers, Authors & Contributors
// Copyright (C) 2016-2021 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

#if __has_include(<linux/input-event-codes.h>)
	#include <linux/input-event-codes.h>
#endif

#ifndef EV_SND
	#define EV_SND 0
#endif

#ifndef SND_TONE
	#define SND_TONE 0
#endif

/// Event device file structure.
struct ircd::beep::ctrl
{
	struct timeval tv {0};
	uint16_t type {EV_SND};
	uint16_t code {SND_TONE};
	int32_t tone {0};
};

decltype(ircd::beep::fd_opts)
ircd::beep::fd_opts
{
	.mode = std::ios::out
};

decltype(ircd::beep::mutex)
ircd::beep::mutex;

decltype(ircd::beep::path)
ircd::beep::path
{
	{ "name",     "ircd.beep.path"                                },
	{ "default",  "/dev/input/by-path/platform-pcspkr-event-spkr" },
};

decltype(ircd::beep::debug)
ircd::beep::debug
{
	{ "name",     "ircd.beep.debug" },
	{ "default",  false             },
};

/// Is another ircd::ctx currently beeping?
bool
ircd::beep::busy()
noexcept
{
	return mutex.locked();
}

/// Is beeping at all possible for this platform?
bool
ircd::beep::available()
noexcept try
{
	return path && fs::exists(path);
}
catch(const std::exception &e)
{
	log::derror
	{
		"Failed to detect PC Speaker event availability :%s",
		e.what()
	};

	return false;
}

//
// beep::beep
//

ircd::beep::beep(const float tone)
try
:lock
{
	tone > 0.0f?
		std::unique_lock<ctx::mutex>{mutex}:
		std::unique_lock<ctx::mutex>{mutex, std::defer_lock}
}
,fd
{
	tone > 0.0f?
		fs::fd{string_view{path}, fd_opts}:
		fs::fd{-1}
}
{
	if(!fd)
		return;

	beep::ctrl c;
	c.tone = tone;
	syscall(::write, int(fd), &c, sizeof(c));

	if(debug)
		log::debug
		{
			"PC Speaker audible tone active @ %-1.1f Hz",
			tone,
		};
}
catch(const std::exception &e)
{
	log::error
	{
		"Failed to activate audible alarm :%s",
		e.what(),
	};

	throw;
}

ircd::beep::beep(beep &&other)
noexcept
:lock{std::move(other.lock)}
,fd{std::move(other.fd)}
{
}

ircd::beep &
ircd::beep::operator=(beep &&other)
noexcept
{
	this->~beep();
	lock = std::move(other.lock);
	fd = std::move(other.fd);
	return *this;
}

ircd::beep::~beep()
noexcept try
{
	if(!fd)
		return;

	assert(mutex.locked());
	assert(this->lock.owns_lock());

	beep::ctrl c;
	c.tone = 0;
	syscall(::write, int(fd), &c, sizeof(c));
}
catch(const std::exception &e)
{
	log::derror
	{
		"Failed to clear pcspkr event (%p)",
		this,
	};

	return;
}
