// The Construct
//
// Copyright (C) The Construct Developers, Authors & Contributors
// Copyright (C) 2016-2020 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

#pragma once
#define HAVE_IRCD_ICU_H

/// International Components for Unicode
namespace ircd::icu
{
	IRCD_EXCEPTION(ircd::error, error)

	int8_t category(const char32_t &) noexcept;
	int16_t block(const char32_t &) noexcept;
	bool is_nonchar(const char32_t &) noexcept;
	bool is_char(const char32_t &) noexcept;

	i32x16 category(const c32x16) noexcept;
	i32x16 block(const c32x16) noexcept;
	u32x16 is_nonchar(const c32x16) noexcept; // returns mask
	u32x16 is_char(const c32x16) noexcept; // returns mask

	char32_t toupper(const char32_t &) noexcept;
	char32_t tolower(const char32_t &) noexcept;

	char32_t name(std::nothrow_t, const string_view &name); // error = U+FFFD
	char32_t name(const string_view &name);

	string_view name(const mutable_buffer &, std::nothrow_t, const char32_t &);
	string_view name(const mutable_buffer &, const char32_t &);

	string_view property_name(const uint &prop);
	string_view property_acronym(const uint &prop);

	extern const info::versions version_api, version_abi;
	extern const info::versions unicode_version_api, unicode_version_abi;
}

namespace ircd::icu::utf8
{
	bool lead(const char &) noexcept;
	bool trail(const char &) noexcept;
	bool single(const char &) noexcept;
	size_t length(const char32_t &) noexcept;
	size_t length(const string_view &) noexcept;
	char32_t get(const string_view &) noexcept;          // error < 0
	char32_t get_or_fffd(const string_view &) noexcept;  // error = U+FFFD
	char32_t get_unsafe(const string_view &) noexcept;   // error undefined
	size_t decode(char32_t *const &out, const size_t &max, const string_view &in);
	const_buffer encode(const mutable_buffer &out, const vector_view<const char32_t> &in);
}

namespace ircd::icu::utf16
{
	bool lead(const char &) noexcept;
	bool trail(const char &) noexcept;
	bool single(const char &) noexcept;
	size_t length(const char32_t &) noexcept;
	size_t length(const string_view &) noexcept;
	char32_t get(const string_view &) noexcept;          // error < 0
	char32_t get_or_fffd(const string_view &) noexcept;  // error = U+FFFD
	char32_t get_unsafe(const string_view &) noexcept;   // error undefined
}
