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
#define HAVE_IRCD_MODS_MODULE_H

namespace ircd::mods
{
	struct module;
}

struct ircd::mods::module
:std::shared_ptr<mod>
{
	operator const mod &() const;
	operator mod &();

	string_view name() const;
	string_view path() const;

	bool has(const string_view &sym) const;

	template<class T = uint8_t> const T *ptr(const string_view &sym) const noexcept;
	template<class T = uint8_t> T *ptr(const string_view &sym) noexcept;

	template<class T> const T &get(const string_view &sym) const;
	template<class T> T &get(const string_view &sym);

	module(std::shared_ptr<mod> ptr = {});
	module(const string_view &name);
};

inline
ircd::mods::module::module(std::shared_ptr<mod> ptr)
:std::shared_ptr<mod>{std::move(ptr)}
{}

template<class T>
T &
ircd::mods::module::get(const string_view &sym)
{
	mod &mod(*this);
	return mods::get<T>(mod, sym);
}

template<class T>
const T &
ircd::mods::module::get(const string_view &sym)
const
{
	const mod &mod(*this);
	return mods::get<T>(mod, sym);
}

template<class T>
T *
ircd::mods::module::ptr(const string_view &sym)
noexcept
{
	mod &mod(*this);
	return mods::ptr<T>(mod, sym);
}

template<class T>
const T *
ircd::mods::module::ptr(const string_view &sym)
const noexcept
{
	const mod &mod(*this);
	return mods::ptr<T>(mod, sym);
}

inline ircd::mods::module::operator
mod &()
{
	assert(bool(*this));
	return std::shared_ptr<mod>::operator*();
}

inline ircd::mods::module::operator
const mod &()
const
{
	assert(bool(*this));
	return std::shared_ptr<mod>::operator*();
}
