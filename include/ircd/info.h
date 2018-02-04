// Matrix Construct
//
// Copyright (C) Matrix Construct Developers, Authors & Contributors
// Copyright (C) 2016-2018 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.


#pragma once
#define HAVE_IRCD_INFO_H

/// Information & metadata about the library.
namespace ircd::info
{
	struct line;
	struct tc_version;

	extern const std::vector<info::line> myinfo;
	extern const std::vector<std::string> credits;
	extern const std::string serno;
	extern const std::string version;
	extern const char *const ircd_version;             // legacy
	extern const uint boost_version[3];

	extern const time_t configured_time;
	extern const time_t compiled_time;
	extern const time_t startup_time;
	extern const std::string compiled;
	extern const std::string configured;
	extern const std::string startup;

	extern const size_t max_align;
	extern const size_t hardware_concurrency;
	extern const size_t destructive_interference;
	extern const size_t constructive_interference;

	#ifdef HAVE_SYS_UTSNAME_H
	extern const ::utsname utsname;
	#endif

	void init();
}

struct ircd::info::line
{
	std::string key;
	std::string valstr;
	uint64_t valnum;
	std::string desc;
};
