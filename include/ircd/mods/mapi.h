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

#define IRCD_MODULE_HEADER_SYMBOL_NAME      "ircd_module"
#define IRCD_MODULE_EXPORT_CODE_SECTION     "ircd.code"
#define IRCD_MODULE_EXPORT_CODE_VISIBILITY  "default"
#define IRCD_MODULE_EXPORT_DATA_SECTION     "ircd.data"
#define IRCD_MODULE_EXPORT_DATA_VISIBILITY  "default"

/// Macro of decorations used on function definitions in modules which are
/// published for demangling and use by libircd.
#define IRCD_MODULE_EXPORT_CODE \
	__attribute__((section(IRCD_MODULE_EXPORT_CODE_SECTION))) \
	__attribute__((visibility(IRCD_MODULE_EXPORT_CODE_VISIBILITY)))

/// Macro of decorations used on object definitions in modules which are
/// published for demangling and use by libircd (object equivalent).
#define IRCD_MODULE_EXPORT_DATA \
	__attribute__((section(IRCD_MODULE_EXPORT_DATA_SECTION))) \
	__attribute__((visibility(IRCD_MODULE_EXPORT_CODE_VISIBILITY)))

// Common convenience aliases
#define IRCD_MODULE_EXPORT \
	IRCD_MODULE_EXPORT_CODE

/// Module declaration
#define IRCD_MODULE \
	__attribute__((visibility("default"))) \
	ircd_module

/// Module API: Interface for module developers.
namespace ircd::mapi
{
	struct header;
	struct metablock;
	using magic_t = uint32_t;
	using version_t = uint16_t;
	using serial_t = uint16_t;
	using meta_data = std::map<string_view, string_view, std::less<>>;
	using init_func = std::function<void ()>;
	using fini_func = std::function<void ()>;

	/// Used to communicate whether a module unload actually took place. dlclose() is allowed to return
	/// success but the actual static destruction of the module's contents doesn't lie. (mods.cc)
	extern bool static_destruction;

	/// The name of the header variable in the module must match this string
	const char *const header_symbol_name
	{
		IRCD_MODULE_HEADER_SYMBOL_NAME
	};

	/// Symbols in these sections are automatically demangle-mapped on load.
	extern const char *const import_section_names[];
}

/// The magic number at the front of the header
constexpr const ircd::mapi::magic_t
IRCD_MAPI_MAGIC
{
	0x112Cd
};

/// The version number of this module's header.
constexpr const ircd::mapi::version_t
IRCD_MAPI_VERSION
{
	4
};

/// The serial number recorded by the module header. We increment this number
/// after removing a module from the project because that module will still
/// remain in the user's install directory. The removed module's serial nr
/// will not be incremented anymore; libircd can ignore modules with serial
/// numbers < this value.
constexpr const ircd::mapi::serial_t
IRCD_MAPI_SERIAL
{
	4
};

/// Module Header
///
/// A static instance of this class must be included in an IRCd module with
/// the unmangled name of IRCD_MODULE (thus there can be only one). It must
/// be externally visible. If this is not present or not visible, ircd::mods
/// will not consider the file to be an IRCd module and it will be ignored.
///
struct ircd::mapi::header
{
	const magic_t magic {IRCD_MAPI_MAGIC};         // The magic must match
	const version_t version {IRCD_MAPI_VERSION};   // Version indicator
	const serial_t serial {IRCD_MAPI_SERIAL};      // Serial indicator
	const int64_t timestamp {RB_TIME_CONFIGURED};  // Module's compile epoch (TODO: XXX)
	std::unique_ptr<metablock> meta;               // Non-standard-layout header data
	mods::mod *self {nullptr};                     // Point to mod instance once loaded

	// get and set metadata
	const string_view &operator[](const string_view &s) const;
	string_view &operator[](const string_view &s);

	// become self
	operator const mods::mod &() const;
	operator mods::mod &();

	header(const string_view &,
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

	metablock(const string_view &, init_func &&, fini_func &&);
};

static_assert
(
	std::is_standard_layout<ircd::mapi::header>(),
	"The MAPI header must be standard layout so the magic and version numbers"
	" can be parsed from the shared object file by external applications."
);

static_assert
(
	sizeof(ircd::mapi::header) == 4 + 4 + 8 + 8 + 8,
	"The MAPI header size has changed on this platform."
);

inline
__attribute__((always_inline))
ircd::mapi::header::header(const string_view &description,
                           init_func init,
                           fini_func fini)
:meta
{
	new metablock
	{
		description, std::move(init), std::move(fini)
	}
}
{}
