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
	std::vector<std::string> sections(const string_view &fullpath);

	std::vector<std::string> symbols(const string_view &fullpath, const string_view &section);
	std::vector<std::string> symbols(const string_view &fullpath);

	std::unordered_map<std::string, std::string> mangles(const std::vector<std::string> &);
	std::unordered_map<std::string, std::string> mangles(const string_view &fullpath, const string_view &section);
	std::unordered_map<std::string, std::string> mangles(const string_view &fullpath);

	// Find module names where symbol resides
	bool has_symbol(const string_view &name, const string_view &symbol);
	std::vector<std::string> find_symbol(const string_view &symbol);
}
