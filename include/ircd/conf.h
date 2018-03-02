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
#define HAVE_IRCD_CONF_H

namespace ircd::conf
{
	template<class T = void> struct item;  // doesn't exist
	template<class T> struct value;        // abstraction for carrying item value
	template<> struct item<void>;          // base class of all conf items
	template<> struct item<seconds>;

	IRCD_EXCEPTION(ircd::error, error)

	extern const std::string &config;  //TODO: X

	void init(const string_view &configfile);
}

/// Conf item base class. You don't create this directly; use one of the
/// derived templates instead.
template<>
struct ircd::conf::item<void>
:instance_list<ircd::conf::item<>>
{
	json::strung feature_;
	json::object feature;
	string_view name;

	virtual bool refresh();

	item(const json::members &);
	virtual ~item() noexcept;
};

/// Conf item value abstraction. If possible, the conf item will also
/// inherit from this template to deduplicate functionality between
/// conf items which contain similar classes of values.
template<class T>
struct ircd::conf::value
:conf::item<>
{
	using value_type = T;

	T _value;
	std::function<bool (T &)> refresher;

	operator const T &() const
	{
		return _value;
	}

	bool refresh() override
	{
		return refresher? refresher(_value) : false;
	}

	template<class U>
	value(const json::members &memb, U&& t)
	:conf::item<>{memb}
	,_value{std::forward<U>(t)}
	{}

	value(const json::members &memb = {})
	:conf::item<>{memb}
	,_value{feature.get<T>("default", T{})}
	{}
};

template<>
struct ircd::conf::item<ircd::seconds>
:conf::value<seconds>
{
	using value_type = seconds;
	using value::value;
};
