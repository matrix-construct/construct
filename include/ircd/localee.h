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
#define HAVE_IRCD_LOCALE_H

/// Utilities for internationalization.
namespace ircd::locale
{

}

// On newer platforms (gcc-5 etc) these conversions are standard C++.
// On older platforms the definition file may use boost::locale.
namespace ircd::locale::char16
{
	char conv(const char16_t &);
	char16_t conv(const char &);

	size_t conv(const char16_t *const &, const size_t &len, char *const &buf, const size_t &max);
	size_t conv(const char *const &, const size_t &len, char16_t *const &buf, const size_t &max);  // uint8_t = max*2

	size_t conv(const char16_t *const &, char *const &buf, const size_t &max);
	size_t conv(const char *const &, char16_t *const &buf, const size_t &max);  // uint8_t = max*2

	std::string conv(const char16_t *const &, const size_t &len);
	std::string conv(const char16_t *const &);
	std::string conv(const std::u16string &);

	std::u16string conv(const char *const &, const size_t &len);
	std::u16string conv(const char *const &);
	std::u16string conv(const std::string &);

	std::ostream &operator<<(std::ostream &, const std::u16string &);
}

namespace ircd::locale
{
	using char16::operator<<;
}

namespace ircd
{
	using locale::operator<<;
}

inline std::ostream &
ircd::locale::char16::operator<<(std::ostream &s,
                                 const std::u16string &u)
{
	s << conv(u);
	return s;
}
