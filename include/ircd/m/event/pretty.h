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
	// fmt = 0: w/o content keys, w/ hashes/sigs
	// fmt = 1: w/ content keys w/ hashes/sigs
	// fmt = 2: w/o content keys w/o hashes/sigs
	std::ostream &pretty_oneline(std::ostream &, const event &, const int &fmt = 1);
	std::string pretty_oneline(const event &, const int &fmt = 1);

	// Informational pretty string on multiple lines.
	std::ostream &pretty(std::ostream &, const event &);
	std::string pretty(const event &);

	// Informational content-oriented
	std::ostream &pretty_msgline(std::ostream &, const event &, const int &fmt = 0);
	std::string pretty_msgline(const event &, const int &fmt = 0);

	// Informational pretty for state
	// io=true will run db queries to enhance w/ more information.
	std::ostream &pretty_stateline(std::ostream &, const event &, const event::idx & = 0);

	// Pretty detailed information; not so pretty right now though...
	// note: lots of queries.
	std::ostream &pretty_detailed(std::ostream &, const event &, const event::idx &);
}
