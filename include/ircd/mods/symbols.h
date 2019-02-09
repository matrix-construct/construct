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

// Section & Symbol utilites
//
// All path parameters can be absolute paths to any module file, or relative
// names which will be found using the mods::paths system.

namespace ircd::mods
{
	// Get all section names
	std::vector<std::string> sections(const string_view &path);

	// Get all symbol names (mangled)
	std::vector<std::string> symbols(const string_view &path, const string_view &section);
	std::vector<std::string> symbols(const string_view &path);

	// Generate demangled -> mangled map
	std::map<std::string, std::string> mangles(const std::vector<std::string> &);
	std::map<std::string, std::string> mangles(const string_view &path, const string_view &section);
	std::map<std::string, std::string> mangles(const string_view &path);

	// Test if module has (mangled) symbol (optionally in section)
	bool has_symbol(const string_view &name, const string_view &symbol, const string_view &section = {});

	// Find module names where (mangled) symbol resides in the mods::paths
	std::vector<std::string> find_symbol(const string_view &symbol, const string_view &section = {});
}
