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
#define HAVE_IRCD_MODS_IMPORT_SHARED_H

namespace ircd::mods
{
	template<class T> struct import_shared;
}

/// Convenience for importing an std::shared_ptr<T> from a loaded lib
///
template<class T>
struct ircd::mods::import_shared
:import<std::shared_ptr<T>>
,std::shared_ptr<T>
{
	using std::shared_ptr<T>::get;
	using std::shared_ptr<T>::operator bool;
	using std::shared_ptr<T>::operator->;
	using std::shared_ptr<T>::operator*;

	operator const T &() const                   { return std::shared_ptr<T>::operator*();         }
	operator T &()                               { return std::shared_ptr<T>::operator*();         }

	import_shared() = default;
	import_shared(module, const string_view &symname);
	import_shared(const string_view &modname, const string_view &symname);
};

template<class T>
ircd::mods::import_shared<T>::import_shared(const string_view &modname,
                                            const string_view &symname)
:import_shared
{
	module(modname), symname
}
{}

template<class T>
ircd::mods::import_shared<T>::import_shared(module module,
                                            const string_view &symname)
:import<std::shared_ptr<T>>
{
	module, symname
}
,std::shared_ptr<T>
{
	import<std::shared_ptr<T>>::operator*()
}{}
