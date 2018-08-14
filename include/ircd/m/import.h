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
#define HAVE_IRCD_M_IMPORT_H

namespace ircd::m
{
	template<class prototype> struct import;

	extern std::map<std::string, ircd::module, std::less<>> imports;
}

template<class prototype>
struct ircd::m::import
:mods::import<prototype>
{
	std::string modname;
	std::string symname;

	void reload();

  public:
	template<class... args> auto operator()(args&&... a);

	import(std::string modname, std::string symname);
};

template<class prototype>
ircd::m::import<prototype>::import(std::string modname,
                                   std::string symname)
:mods::import<prototype>{}
,modname{std::move(modname)}
,symname{std::move(symname)}
{}

template<class prototype>
template<class... args>
auto
ircd::m::import<prototype>::operator()(args&&... a)
{
	if(unlikely(!*this))
		reload();

	return mods::import<prototype>::operator()(std::forward<args>(a)...);
}

template<class prototype>
void
ircd::m::import<prototype>::reload()
try
{
	auto &import(static_cast<mods::import<prototype> &>(*this));
	import = { imports.at(modname), symname };
}
catch(const std::out_of_range &e)
{
	throw m::UNAVAILABLE
	{
		"Sorry, %s in %s is currently unavailable.",
		symname,
		modname
	};
}
