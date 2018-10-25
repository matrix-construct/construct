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
	struct metablock;
	using magic_t = uint16_t;
	using version_t = uint16_t;
	using meta_data = std::map<string_view, string_view, std::less<>>;
	using init_func = std::function<void ()>;
	using fini_func = std::function<void ()>;

	/// Used to communicate whether a module unload actually took place. dlclose() is allowed to return
	/// success but the actual static destruction of the module's contents doesn't lie. (mods.cc)
	extern bool static_destruction;

	/// The name of the header variable in the module must match this string
	const char *const header_symbol_name
	{
		"IRCD_MODULE"
	};
}

/// The magic number at the front of the header
constexpr const ircd::mapi::magic_t
IRCD_MAPI_MAGIC
{
	0x4D41
};

/// The version number of this module's header.
constexpr const ircd::mapi::version_t
IRCD_MAPI_VERSION
{
	4
};

struct ircd::mapi::header
{
	const magic_t magic {IRCD_MAPI_MAGIC};       // The magic must match
	const version_t version {IRCD_MAPI_VERSION}; // Version indicator
	const int32_t _reserved_ {0};                // MBZ
	const int64_t timestamp {RB_DATECODE};       // Module's compile epoch
	std::unique_ptr<metablock> meta;             // Non-standard-layout header data
	mods::mod *self {nullptr};                   // Point to mod instance once loaded

	// get and set metadata
	const string_view &operator[](const string_view &s) const;
	string_view &operator[](const string_view &s);

	// become self
	operator const mods::mod &() const;
	operator mods::mod &();

	header(const string_view &  = "<no description>",
	       init_func            = {},
	       fini_func            = {});

	header(header &&) = delete;
	header(const header &) = delete;
	~header() noexcept;
};

struct ircd::mapi::metablock
{
	init_func init;                    // Executed after dlopen()
	fini_func fini;                    // Executed before dlclose()
	meta_data meta;                    // Various key-value metadata
};

inline
ircd::mapi::header::header(const string_view &description,
                           init_func init,
                           fini_func fini)
:meta
{
	new metablock
	{
		std::move(init), std::move(fini), meta_data
		{
			{ "description", description }
		}
	}
}
{
}

inline
ircd::mapi::header::~header()
noexcept
{
	static_destruction = true;
}

static_assert
(
	std::is_standard_layout<ircd::mapi::header>(),
	"The MAPI header must be standard layout so the magic and version numbers"
	" can be parsed from the shared object file by external applications."
);

static_assert
(
	sizeof(ircd::mapi::header) == 2 + 2 + 4 + 8 + 8 + 8,
	"The MAPI header size has changed on this platform."
);
