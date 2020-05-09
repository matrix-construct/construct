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
#define HAVE_IRCD_PROF_PSI_H

namespace ircd::prof::psi
{
	struct file;
	struct metric;
	struct trigger;

	// Read and update the referenced extern.
	bool refresh(file &) noexcept;

	// Yield ircd::ctx until event; returns unrefreshed
	file &wait(const vector_view<const trigger> & = {});

	extern const bool supported;
	extern const string_view path[3];
	extern file cpu, mem, io;
}

struct ircd::prof::psi::trigger
{
	const psi::file &file;
	string_view string;
};

struct ircd::prof::psi::metric
{
	struct avg
	{
		seconds window;
		float pct;
	};

	struct
	{
		microseconds total;            // stall value direct from system
		microseconds relative;         // value since last sample only
		microseconds window;           // duration since last sample
		float pct;                     // % of stall time since last sample
	}
	stall;
	std::array<struct avg, 3> avg;
};

struct ircd::prof::psi::file
{
	string_view name;
	system_point sampled;
	metric some, full;
};

#ifndef __linux__
inline bool
ircd::prof::psi::refresh(metric &)
noexcept
{
	return false; // unsupported platform
}
#endif
