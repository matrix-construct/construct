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
#define HAVE_IRCD_MODS_SYM_PTR_H

namespace ircd::mods
{
	struct sym_ptr;
}

/// Representation of a symbol in a loaded library (non-template; low level).
///
class ircd::mods::sym_ptr
:std::weak_ptr<mod>
{
	void *ptr {nullptr};

  public:
	bool operator!() const;
	explicit operator bool() const;

	template<class T> const T *get() const;
	template<class T> const T *operator->() const;
	template<class T> const T &operator*() const;
	template<class T, class... args> auto operator()(args&&... a) const;

	template<class T> T *get();
	template<class T> T *operator->();
	template<class T> T &operator*();
	template<class T, class... args> auto operator()(args&&... a);

	explicit sym_ptr(mod &, const string_view &symname);
	sym_ptr(module, const string_view &symname);
	sym_ptr(const string_view &modname, const string_view &symname);
	sym_ptr() = default;
};

template<class T,
         class... args>
auto
ircd::mods::sym_ptr::operator()(args&&... a)
{
	return (operator*<T>())(std::forward<args>(a)...);
}

template<class T>
T &
ircd::mods::sym_ptr::operator*()
{
	if(unlikely(expired()))
		throw expired_symbol
		{
			"The reference to a symbol in another module is no longer valid"
		};

	return *operator-><T>();
}

template<class T>
T *
ircd::mods::sym_ptr::operator->()
{
	return get<T>();
}

template<class T>
T *
ircd::mods::sym_ptr::get()
{
	return reinterpret_cast<T *>(ptr);
}

template<class T,
         class... args>
auto
ircd::mods::sym_ptr::operator()(args&&... a)
const
{
	return (operator*<T>())(std::forward<args>(a)...);
}

template<class T>
const T &
ircd::mods::sym_ptr::operator*()
const
{
	if(unlikely(expired()))
		throw expired_symbol
		{
			"The const reference to a symbol in another module is no longer valid"
		};

	return *operator-><T>();
}

template<class T>
const T *
ircd::mods::sym_ptr::operator->()
const
{
	return get<T>();
}

template<class T>
const T *
ircd::mods::sym_ptr::get()
const
{
	return reinterpret_cast<const T *>(ptr);
}

inline ircd::mods::sym_ptr::operator
bool()
const
{
	return !operator!();
}

inline bool
ircd::mods::sym_ptr::operator!()
const
{
	return !ptr || expired();
}
