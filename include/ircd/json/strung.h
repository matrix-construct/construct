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
#define HAVE_IRCD_JSON_STRUNG_H

namespace ircd::json
{
	struct strung;
}

/// Interface around an allocated std::string of JSON. This is not a
/// json::string type; it is a full JSON which will stringify the
/// constructor arguments into the std::string's buffer as a convenience
/// to reduce boilerplate when the user has no interest in buffering the
/// stringify() calls another way.
/// 
struct ircd::json::strung
:std::string
{
	explicit operator json::object() const;
	explicit operator json::array() const;

	strung() = default;
	template<class... T> strung(T&&... t);
	strung(strung &&s) noexcept;
	strung(const strung &s);
	strung &operator=(strung &&) = default;
	strung &operator=(const strung &) = default;
};

inline
ircd::json::strung::strung(strung &&s)
noexcept
:std::string{std::move(s)}
{}

inline
ircd::json::strung::strung(const strung &s)
:std::string{s}
{}

template<class... T>
ircd::json::strung::strung(T&&... t)
:std::string
{
	util::string(serialized(std::forward<T>(t)...), [&t...]
	(const mutable_buffer &out)
	{
		const auto sv
		{
			stringify(mutable_buffer{out}, std::forward<T>(t)...)
		};

		valid_output(sv, ircd::size(out));
		return sv;
	})
}
{}
