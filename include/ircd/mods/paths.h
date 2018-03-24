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
#define HAVE_IRCD_MODS_PATHS_H

namespace ircd::mods
{
	struct paths extern paths;

	// Platform (.so|.dll) postfixing
	std::string postfixed(const std::string &name);
	std::string unpostfixed(const std::string &name);
}

struct ircd::mods::paths
:std::vector<std::string>
{
	bool added(const std::string &dir) const;

	bool del(const std::string &dir);
	bool add(const std::string &dir, std::nothrow_t);
	bool add(const std::string &dir);

	paths();
};
