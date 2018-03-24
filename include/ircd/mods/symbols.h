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
#define HAVE_IRCD_MODS_SYMBOLS_H

namespace ircd::mods
{
	// Section & Symbol utilites
	std::vector<std::string> sections(const std::string &fullpath);

	std::vector<std::string> symbols(const std::string &fullpath, const std::string &section);
	std::vector<std::string> symbols(const std::string &fullpath);

	std::unordered_map<std::string, std::string> mangles(const std::vector<std::string> &);
	std::unordered_map<std::string, std::string> mangles(const std::string &fullpath, const std::string &section);
	std::unordered_map<std::string, std::string> mangles(const std::string &fullpath);

	// Find module names where symbol resides
	bool has_symbol(const std::string &name, const std::string &symbol);
	std::vector<std::string> find_symbol(const std::string &symbol);
}
