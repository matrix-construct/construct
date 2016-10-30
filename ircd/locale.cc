/*
 * charybdis: standing on the shoulders of giant build times
 *
 * Copyright (C) 2016 Charybdis Development Team
 * Copyright (C) 2016 Jason Volk <jason@zemos.net>
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice is present in all copies.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

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
size_t
ircd::locale::char16::conv(const char16_t *const &str,
                           char *const &buf,
                           const size_t &max)
{
	static std::wstring_convert<std::codecvt_utf8<char16_t>, char16_t> converter;

	//TODO: optimize
	const auto s(converter.to_bytes(str));
	return rb_strlcpy(buf, s.c_str(), max);
}
#else
size_t
ircd::locale::char16::conv(const char16_t *const &str,
                      char *const &buf,
                      const size_t &max)
{
	//TODO: optimize
	const auto s(boost::locale::conv::utf_to_utf<char>(str));
	return rb_strlcpy(buf, s.c_str(), max);
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
