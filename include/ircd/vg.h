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

	[[gnu::hot]] size_t errors() noexcept;
	[[gnu::hot]] bool defined(const void *, const size_t) noexcept;
	template<class T> bool defined(const T *const &, const size_t & = sizeof(T));
	bool defined(const const_buffer &);

	void set_defined(const const_buffer) noexcept;
	void set_undefined(const const_buffer) noexcept;
	void set_noaccess(const const_buffer) noexcept;
}

namespace ircd::vg::stack
{
	uint add(const mutable_buffer) noexcept;
	void del(const uint id) noexcept;
}

inline bool
ircd::vg::defined(const const_buffer &buf)
{
	return vg::defined(data(buf), size(buf));
}

template<class T>
inline bool
ircd::vg::defined(const T *const &t,
                  const size_t &size)
{
	return vg::defined(static_cast<const void *>(t), size);
}
