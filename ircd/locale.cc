// Matrix Construct
//
// Copyright (C) Matrix Construct Developers, Authors & Contributors
// Copyright (C) 2016-2018 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

#include <boost/locale.hpp>

namespace ircd   {
namespace locale {

} // namespace locale
} // namespace ircd

#ifdef HAVE_CODECVT
std::u16string
ircd::locale::char16::conv(const std::string &s)
{
	static std::wstring_convert<std::codecvt_utf8<char16_t>, char16_t> converter;
	return converter.from_bytes(s);
}
#else
std::u16string
ircd::locale::char16::conv(const std::string &s)
{
	return boost::locale::conv::utf_to_utf<char16_t>(s);
}
#endif

#ifdef HAVE_CODECVT
std::u16string
ircd::locale::char16::conv(const char *const &s)
{
	static std::wstring_convert<std::codecvt_utf8<char16_t>, char16_t> converter;
	return s? converter.from_bytes(s) : std::u16string{};
}
#else
std::u16string
ircd::locale::char16::conv(const char *const &s)
{
	return boost::locale::conv::utf_to_utf<char16_t>(s);
}
#endif

#ifdef HAVE_CODECVT
std::u16string
ircd::locale::char16::conv(const char *const &s,
                           const size_t &len)
{
	static std::wstring_convert<std::codecvt_utf8<char16_t>, char16_t> converter;
	return s && len? converter.from_bytes(s, s + len) : std::u16string{};
}
#else
std::u16string
ircd::locale::char16::conv(const char *const &s,
                           const size_t &len)
{
	return boost::locale::conv::utf_to_utf<char16_t>(s, s + len);
}
#endif

#ifdef HAVE_CODECVT
std::string
ircd::locale::char16::conv(const std::u16string &s)
{
	static std::wstring_convert<std::codecvt_utf8<char16_t>, char16_t> converter;
	return converter.to_bytes(s);
}
#else
std::string
ircd::locale::char16::conv(const std::u16string &s)
{
	return boost::locale::conv::utf_to_utf<char>(s);
}
#endif

#ifdef HAVE_CODECVT
std::string
ircd::locale::char16::conv(const char16_t *const &s)
{
	static std::wstring_convert<std::codecvt_utf8<char16_t>, char16_t> converter;
	return s? converter.to_bytes(s) : std::string{};
}
#else
std::string
ircd::locale::char16::conv(const char16_t *const &s)
{
	return boost::locale::conv::utf_to_utf<char>(s);
}
#endif

#ifdef HAVE_CODECVT
std::string
ircd::locale::char16::conv(const char16_t *const &s,
                           const size_t &len)
{
	static std::wstring_convert<std::codecvt_utf8<char16_t>, char16_t> converter;
	return s && len? converter.to_bytes(s, s + len) : std::string{};
}
#else
std::string
ircd::locale::char16::conv(const char16_t *const &s,
                           const size_t &len)
{
	return boost::locale::conv::utf_to_utf<char>(s, s + len);
}
#endif

#ifdef HAVE_CODECVT
size_t
ircd::locale::char16::conv(const char16_t *const &str,
                           char *const &buf,
                           const size_t &max)
{
	static std::wstring_convert<std::codecvt_utf8<char16_t>, char16_t> converter;

	//TODO: optimize
	const auto s(converter.to_bytes(str));
	return strlcpy(buf, s.c_str(), max);
}
#else
size_t
ircd::locale::char16::conv(const char16_t *const &str,
                           char *const &buf,
                           const size_t &max)
{
	//TODO: optimize
	const auto s(boost::locale::conv::utf_to_utf<char>(str));
	return strlcpy(buf, s.c_str(), max);
}
#endif

#ifdef HAVE_CODECVT
size_t
ircd::locale::char16::conv(const char16_t *const &str,
                           const size_t &len,
                           char *const &buf,
                           const size_t &max)
{
	static std::wstring_convert<std::codecvt_utf8<char16_t>, char16_t> converter;

	//TODO: optimize
	const auto s(converter.to_bytes(str, str + len));
	return strlcpy(buf, s.c_str(), max);
}
#else
size_t
ircd::locale::char16::conv(const char16_t *const &str,
                           const size_t &len,
                           char *const &buf,
                           const size_t &max)
{
	//TODO: optimize
	const auto s(boost::locale::conv::utf_to_utf<char>(str, str + len));
	return strlcpy(buf, s.c_str(), max);
}
#endif

#ifdef HAVE_CODECVT
size_t
ircd::locale::char16::conv(const char *const &str,
                           char16_t *const &buf,
                           const size_t &max)
{
	static std::wstring_convert<std::codecvt_utf8<char16_t>, char16_t> converter;

	if(unlikely(!max))
		return 0;

	//TODO: optimize
	const auto s(converter.from_bytes(str));
	const auto cpsz(std::min(s.size(), size_t(max - 1)));
	memcpy(buf, s.data(), cpsz * 2);
	buf[cpsz] = char16_t(0);
	return cpsz;
}
#else
size_t
ircd::locale::char16::conv(const char *const &str,
                           char16_t *const &buf,
                           const size_t &max)
{
	if(unlikely(!max))
		return 0;

	//TODO: optimize
	const auto s(boost::locale::conv::utf_to_utf<char16_t>(str));
	const auto cpsz(std::min(s.size(), size_t(max - 1)));
	memcpy(buf, s.data(), cpsz * 2);
	buf[cpsz] = char16_t(0);
	return cpsz;
}
#endif

#ifdef HAVE_CODECVT
size_t
ircd::locale::char16::conv(const char *const &str,
                           const size_t &len,
                           char16_t *const &buf,
                           const size_t &max)
{
	static std::wstring_convert<std::codecvt_utf8<char16_t>, char16_t> converter;

	if(unlikely(!max))
		return 0;

	//TODO: optimize
	const auto s(converter.from_bytes(str, str + len));
	const auto cpsz(std::min(s.size(), size_t(max - 1)));
	memcpy(buf, s.data(), cpsz * 2);
	buf[cpsz] = char16_t(0);
	return cpsz;
}
#else
size_t
ircd::locale::char16::conv(const char *const &str,
                           const size_t &len,
                           char16_t *const &buf,
                           const size_t &max)
{
	if(unlikely(!max))
		return 0;

	//TODO: optimize
	const auto s(boost::locale::conv::utf_to_utf<char16_t>(str, str + len));
	const auto cpsz(std::min(s.size(), size_t(max - 1)));
	memcpy(buf, s.data(), cpsz * 2);
	buf[cpsz] = char16_t(0);
	return cpsz;
}
#endif

char16_t
ircd::locale::char16::conv(const char &c)
{
	//TODO: optimize
	char16_t ret[2];
	char cs[2] = { c, '\0' };
	conv(cs, ret, 1);
	return ret[0];
}

char
ircd::locale::char16::conv(const char16_t &c)
{
	//TODO: optimize
	char ret[2];
	char16_t cs[2] = { c, char16_t(0) };
	conv(cs, ret, 1);
	return ret[0];
}
