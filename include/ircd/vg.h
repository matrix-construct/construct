// The Construct
//
// Copyright (C) The Construct Developers, Authors & Contributors
// Copyright (C) 2016-2020 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

#pragma once
#define HAVE_IRCD_VG_H

/// Valgrind memcheck hypercall suite
namespace ircd::vg
{
	extern const bool active;
	size_t errors() noexcept;

	bool defined(const const_buffer &) noexcept;
	void set_defined(const const_buffer &) noexcept;
	void set_undefined(const const_buffer &) noexcept;
	void set_noaccess(const const_buffer &) noexcept;
}

namespace ircd::vg::stack
{
	uint add(const mutable_buffer &) noexcept;
	void del(const uint &id) noexcept;
}
