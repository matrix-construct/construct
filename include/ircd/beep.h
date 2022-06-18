// Matrix Construct
//
// Copyright (C) Matrix Construct Developers, Authors & Contributors
// Copyright (C) 2016-2021 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

#pragma once
#define HAVE_IRCD_BEEP_H

/// pcspkr-event-spkr
namespace ircd
{
	struct beep;
}

/// Transmits audible tone through the pcspkr for the object's duration. The
/// tone starts at construction and continues until terminated by destruction.
///
class ircd::beep
{
	struct ctrl;

	static const fs::fd::opts fd_opts;
	static ctx::mutex mutex;
	static conf::item<std::string> path;
	static conf::item<bool> debug;

	std::unique_lock<ctx::mutex> lock;
	fs::fd fd;

  public:
	beep(const float tone = 2600.0f);
	beep(beep &&) noexcept;
	beep(const beep &) = delete;
	beep &operator=(beep &&) noexcept;
	beep &operator=(const beep &) = delete;
	~beep() noexcept;

	static bool available() noexcept;
	static bool busy() noexcept;
};
