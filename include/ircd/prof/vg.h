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
#define HAVE_IRCD_PROF_VG_H

/// Callgrind hypercall suite
namespace ircd::prof::vg
{
	struct enable;
	struct disable;

	bool enabled();
	void dump(const char *const reason = nullptr);
	void toggle();
	void reset();
	void start() noexcept;
	void stop() noexcept;
}

/// Enable callgrind profiling for the scope
struct ircd::prof::vg::enable
{
	enable() noexcept;
	~enable() noexcept;
};

/// Disable any enabled callgrind profiling for the scope; then restore.
struct ircd::prof::vg::disable
{
	disable() noexcept;
	~disable() noexcept;
};
