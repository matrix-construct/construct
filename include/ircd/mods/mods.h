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
	IRCD_EXCEPTION(expired_symbol, unavailable)

	string_view name(const mod &);
	string_view path(const mod &);
	bool loaded(const mod &);
	bool loading(const mod &);
	bool unloading(const mod &);
	bool has(const mod &, const string_view &sym);
	template<class T = uint8_t> const T *ptr(const mod &, const string_view &sym);
	template<class T = uint8_t> T *ptr(mod &, const string_view &sym);
	template<class T> const T &get(const mod &, const string_view &sym);
	template<class T> T &get(mod &, const string_view &sym);

	// Utils by name
	bool loaded(const string_view &name);
	bool loading(const string_view &name);
	bool unloading(const string_view &name);
	bool available(const string_view &name);

	// Utils by path
	bool is_module(const string_view &fullpath);
	bool is_module(const string_view &fullpath, std::nothrow_t);
	bool is_module(const string_view &fullpath, std::string &why);

	// returns dir/name of first dir containing 'name' (and this will be
	// a loadable module). Unlike libltdl, the reason each individual
	// candidate failed is presented in a vector.
	std::string search(const string_view &name, std::vector<std::string> &why);
	std::string search(const string_view &name);

	// Potential modules available to load
	std::forward_list<std::string> available();
}

#include "paths.h"
#include "symbols.h"
#include "module.h"
#include "sym_ptr.h"
#include "import.h"
#include "import_shared.h"

// Exports down into ircd::
namespace ircd
{
	using mods::module;
	using mods::import;
	using mods::import_shared;
}

namespace ircd::mods
{
	template<> const uint8_t *ptr<const uint8_t>(const mod &, const string_view &sym);
	template<> uint8_t *ptr<uint8_t>(mod &, const string_view &sym);
}

template<class T>
T &
ircd::mods::get(mod &mod,
                const string_view &sym)
{
	return *ptr<T>(mod, sym);
}

template<class T>
const T &
ircd::mods::get(const mod &mod,
                const string_view &sym)
{
	return *ptr<T>(mod, sym);
}

template<class T>
T *
ircd::mods::ptr(mod &mod,
                const string_view &sym)
{
	return reinterpret_cast<T *>(ptr<uint8_t>(mod, sym));
}

template<class T>
const T *
ircd::mods::ptr(const mod &mod,
                const string_view &sym)
{
	return reinterpret_cast<const T *>(ptr<const uint8_t>(mod, sym));
}
