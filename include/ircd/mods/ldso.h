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
#define HAVE_IRCD_MODS_LDSO_H

// This is a platform dependent interface. While this header and these decls
// are unconditionally included in the standard include stack, the defs
// behind them may not be compiled and linked on all platforms.

// Forward declarations for <link.h> because it is not included here.
extern "C"
{
	struct link_map;
}

/// Platform-dependent interface on ELF+ld.so supporting compilations.
namespace ircd::mods::ldso
{
	IRCD_EXCEPTION(mods::error, error)
	IRCD_EXCEPTION(error, not_found);

	using link_closure = std::function<bool (struct link_map &)>;
	using semantic_version = std::array<long, 3>;

	// Util
	string_view fullname(const struct link_map &);          // /lib/x86_64-linux-gnu/libz.so.1
	string_view soname(const string_view &fullname);        // libz.so.1
	string_view soname(const struct link_map &);            // libz.so.1
	string_view name(const string_view &soname);            // z
	string_view name(const struct link_map &);              // z
	semantic_version version(const string_view &soname);    // 1.0.0
	semantic_version version(const struct link_map &map);   // 1.0.0

	// Iteration
	bool for_each(const link_closure &);
	bool has_fullname(const string_view &path);
	bool has_soname(const string_view &name);
	bool has(const string_view &name);
	size_t count();

	// Get link
	struct link_map *get(std::nothrow_t, const string_view &name);
	struct link_map &get(const string_view &name);

	// Query link
	string_view string(const struct link_map &, const size_t &idx);
}
