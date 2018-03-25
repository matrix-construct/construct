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
#define HAVE_IRCD_BYTE_VIEW_H

namespace ircd
{
	template<class T = string_view> struct byte_view;
	template<> struct byte_view<string_view>;
}

/// string_view -> bytes
template<class T>
struct ircd::byte_view
{
	string_view s;

	operator const T &() const
	{
		if(unlikely(sizeof(T) > s.size()))
			throw std::bad_cast();

		return *reinterpret_cast<const T *>(s.data());
	}

	byte_view(const string_view &s = {})
	:s{s}
	{
		if(unlikely(sizeof(T) > s.size()))
			throw std::bad_cast();
	}

	// bytes -> bytes (completeness)
	byte_view(const T &t)
	:s{byte_view<string_view>{t}}
	{}
};

/// bytes -> string_view. A byte_view<string_view> is raw data of byte_view<T>.
///
/// This is an important specialization to take note of. When you see
/// byte_view<string_view> know that another type's bytes are being represented
/// by the string_view if that type is not string_view family itself.
template<>
struct ircd::byte_view<ircd::string_view>
:string_view
{
	template<class T,
	         typename std::enable_if<!std::is_base_of<std::string_view, T>::value, int *>::type = nullptr>
	byte_view(const T &t)
	:string_view{reinterpret_cast<const char *>(&t), sizeof(T)}
	{}

	/// string_view -> string_view (completeness)
	byte_view(const string_view &t = {})
	:string_view{t}
	{}
};
