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
#define HAVE_IRCD_MAPI_H

/// Module API: Interface for module developers.
namespace ircd::mapi
{
	struct header;
	using magic_t = uint16_t;
	using version_t = uint16_t;
	using metadata = std::map<string_view, string_view, std::less<>>;
	using init_function = std::function<void ()>;
	using fini_function = std::function<void ()>;

	const char *const header_symbol_name
	{
		"IRCD_MODULE"
	};

	constexpr const magic_t MAGIC
	{
		0x4D41
	};

	// Used to communicate whether a module unload actually took place. dlclose() is allowed to return
	// success but the actual static destruction of the module's contents doesn't lie. (mods.cc)
	extern bool static_destruction;
}

struct ircd::mapi::header
{
	magic_t magic;                               // The magic must match MAGIC
	version_t version;                           // Version indicator
	int64_t timestamp;                           // Module's compile epoch
	init_function init;                          // Executed after dlopen()
	fini_function fini;                          // Executed before dlclose()
	metadata meta;                               // Various key-value metadata
	mods::mod *self;                             // Module instance once loaded

	// get and set metadata
	auto &operator[](const string_view &s) const;
	auto &operator[](const string_view &s);

	// become self
	operator const mods::mod &() const;
	operator mods::mod &();

	header(const string_view &desc = "<no description>",
	       init_function = {},
	       fini_function = {});

	~header() noexcept;
};

inline
ircd::mapi::header::header(const string_view &desc,
                           init_function init,
                           fini_function fini)
:magic(MAGIC)
,version(4)
,timestamp(RB_DATECODE)
,init{std::move(init)}
,fini{std::move(fini)}
,meta
{
	{ "description", desc }
}
,self{nullptr}
{
}

inline
ircd::mapi::header::~header()
noexcept
{
	static_destruction = true;
}

inline ircd::mapi::header::operator
mods::mod &()
{
	assert(self);
	return *self;
}

inline ircd::mapi::header::operator
const mods::mod &()
const
{
	assert(self);
	return *self;
}

inline auto &
ircd::mapi::header::operator[](const string_view &key)
{
	return meta[key];
}

inline auto &
ircd::mapi::header::operator[](const string_view &key)
const
{
	return meta.at(key);
}
