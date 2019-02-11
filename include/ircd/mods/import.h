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
	struct imports extern imports;

	std::string make_target_name(const string_view &name, const string_view &demangled);
}

struct ircd::mods::imports
:std::map<std::string, mods::module, std::less<>>
{};

/// Representation of a symbol in a loaded shared library
///
template<class T>
struct ircd::mods::import
:sym_ptr
{
	string_view mangled_type;
	std::string demangled_type;
	std::string module_name;
	std::string symbol_name;
	std::string target_name;

	void reload();

  public:
	template<class... args> decltype(auto) operator()(args&&...) const;
	template<class... args> decltype(auto) operator()(args&&...);

	const T *operator->() const;
	const T &operator*() const;
	operator const T &() const;

	T *operator->();
	T &operator*();
	operator T &();

	explicit import(const mods::module &, std::string symbol_name);
	import(std::string module_name, std::string symbol_name);
};

template<class T>
ircd::mods::import<T>::import(std::string module_name,
                              std::string symbol_name)
:sym_ptr
{
	// Note sym_ptr is purposely default constructed; this makes the import
	// "lazy" and will be loaded via miss on first use. This is useful in
	// the general use-case of static construction.
}
,mangled_type
{
	typeid(T).name()
}
,demangled_type
{
	demangle(mangled_type)
}
,module_name
{
	std::move(module_name)
}
,symbol_name
{
	std::move(symbol_name)
}
,target_name
{
	make_target_name(this->symbol_name, demangled_type)
}
{}

template<class T>
ircd::mods::import<T>::import(const mods::module &module,
                              std::string symbol_name)
:sym_ptr
{

}
,mangled_type
{
	typeid(T).name()
}
,demangled_type
{
	demangle(mangled_type)
}
,module_name
{
	module.name()
}
,symbol_name
{
	std::move(symbol_name)
}
,target_name
{
	make_target_name(this->symbol_name, demangled_type)
}
{
	auto &sp
	{
		static_cast<sym_ptr &>(*this)
	};

	sp =
	{
		module, !empty(target_name)? target_name : this->symbol_name
	};
}

template<class T>
ircd::mods::import<T>::operator
T &()
{
	return this->operator*();
}

template<class T>
T &
ircd::mods::import<T>::operator*()
{
	return *this->operator->();
}

template<class T>
T *
ircd::mods::import<T>::operator->()
{
	if(unlikely(!*this))
		reload();

	return sym_ptr::operator-><T>();
}

template<class T>
template<class... args>
decltype(auto)
ircd::mods::import<T>::operator()(args&&... a)
{
	if(unlikely(!*this))
		reload();

	return sym_ptr::operator()<T>(std::forward<args>(a)...);
}

template<class T>
ircd::mods::import<T>::operator
const T &()
const
{
	return this->operator*();
}

template<class T>
const T &
ircd::mods::import<T>::operator*()
const
{
	return this->operator->();
}

template<class T>
const T *
ircd::mods::import<T>::operator->()
const
{
	return sym_ptr::operator-><T>();
}

template<class T>
template<class... args>
decltype(auto)
ircd::mods::import<T>::operator()(args&&... a)
const
{
	return sym_ptr::operator()<T>(std::forward<args>(a)...);
}

template<class T>
void
ircd::mods::import<T>::reload()
try
{
	auto &module
	{
		imports.at(module_name)
	};

	auto &sp
	{
		static_cast<sym_ptr &>(*this)
	};

	const auto &symname
	{
		!empty(target_name)? target_name : symbol_name
	};

	sp = { module, symname };
}
catch(const std::out_of_range &e)
{
	throw unavailable
	{
		"Sorry, %s in %s is currently unavailable.",
		symbol_name,
		module_name
	};
}
