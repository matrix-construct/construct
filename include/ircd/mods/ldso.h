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
	using link_closure = std::function<bool (struct link_map &)>;

	string_view name(const struct link_map &);

	bool for_each(const link_closure &);
	bool has(const string_view &name);
	size_t count();
}
