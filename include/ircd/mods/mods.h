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
#define HAVE_IRCD_MODS_MODS_H

/// Modules system
namespace ircd::mods
{
	struct mod;

	IRCD_EXCEPTION(ircd::error, error)
	IRCD_EXCEPTION(error, filesystem_error)
	IRCD_EXCEPTION(error, invalid_export)
	IRCD_EXCEPTION(error, expired_symbol)
	IRCD_EXCEPTION(error, undefined_symbol)

	string_view name(const mod &);
	string_view path(const mod &);
}

#include "paths.h"
#include "symbols.h"
#include "module.h"
#include "sym_ptr.h"
#include "import.h"
#include "import_shared.h"

// misc util
namespace ircd::mods
{
	bool loaded(const std::string &name);
	bool available(const std::string &name);
	bool is_module(const std::string &fullpath);
	bool is_module(const std::string &fullpath, std::nothrow_t);
	bool is_module(const std::string &fullpath, std::string &why);

	// returns dir/name of first dir containing 'name' (and this will be
	// a loadable module). Unlike libltdl, the reason each individual
	// candidate failed is presented in a vector.
	std::string search(const std::string &name, std::vector<std::string> &why);
	std::string search(const std::string &name);

	// Potential modules available to load
	std::forward_list<std::string> available();
}

// Exports down into ircd::
namespace ircd
{
	using mods::module;
	using mods::import;
	using mods::import_shared;
}
