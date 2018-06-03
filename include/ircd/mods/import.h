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
#define HAVE_IRCD_MODS_IMPORT_H

namespace ircd::mods
{
	template<class T> struct import;
}

/// Representation of a symbol in a loaded shared library
///
template<class T>
struct ircd::mods::import
:sym_ptr
{
	template<class... args>
	auto operator()(args&&... a) const
	{
		return sym_ptr::operator()<T>(std::forward<args>(a)...);
	}

	template<class... args>
	auto operator()(args&&... a)
	{
		return sym_ptr::operator()<T>(std::forward<args>(a)...);
	}

	const T *operator->() const                  { return sym_ptr::operator-><T>();                }
	const T &operator*() const                   { return sym_ptr::operator*<T>();                 }
	operator const T &() const                   { return sym_ptr::operator*<T>();                 }

	T *operator->()                              { return sym_ptr::operator-><T>();                }
	T &operator*()                               { return sym_ptr::operator*<T>();                 }
	operator T &()                               { return sym_ptr::operator*<T>();                 }

	using sym_ptr::sym_ptr;
};
