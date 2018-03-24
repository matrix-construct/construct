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
	const std::string &name() const;
	const std::string &path() const;
	const std::string &mangle(const std::string &) const;

	bool has(const std::string &name) const;

	template<class T = uint8_t> const T *ptr(const std::string &name) const;
	template<class T = uint8_t> T *ptr(const std::string &name);

	template<class T> const T &get(const std::string &name) const;
	template<class T> T &get(const std::string &name);

	module(std::shared_ptr<mod> ptr = {})
	:std::shared_ptr<mod>{std::move(ptr)}
	{}

	module(const std::string &name);
	~module() noexcept;
};

namespace ircd::mods
{
	template<> const uint8_t *module::ptr<const uint8_t>(const std::string &name) const;
	template<> uint8_t *module::ptr<uint8_t>(const std::string &name);
}

template<class T>
T &
ircd::mods::module::get(const std::string &name)
{
	return *ptr<T>(name);
}

template<class T>
const T &
ircd::mods::module::get(const std::string &name)
const
{
	return *ptr<T>(name);
}

template<class T>
T *
ircd::mods::module::ptr(const std::string &name)
{
	return reinterpret_cast<T *>(ptr<uint8_t>(name));
}

template<class T>
const T *
ircd::mods::module::ptr(const std::string &name)
const
{
	return reinterpret_cast<const T *>(ptr<const uint8_t>(name));
}
