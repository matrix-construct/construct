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
#define HAVE_IRCD_M_EVENT_PRETTY_H

namespace ircd::m
{
	// Informational pretty string condensed to single line.
	std::ostream &pretty_oneline(std::ostream &, const event &, const bool &content_keys = true);
	std::string pretty_oneline(const event &, const bool &content_keys = true);

	// Informational pretty string on multiple lines.
	std::ostream &pretty(std::ostream &, const event &);
	std::string pretty(const event &);

	// Informational content-oriented
	std::ostream &pretty_msgline(std::ostream &, const event &);
	std::string pretty_msgline(const event &);
}
