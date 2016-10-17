/*
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

#pragma once
#define HAVE_IRCD_JS_STRING_H

namespace ircd {
namespace js   {

// SpiderMonkey may use utf-16/char16_t strings; these will help you then
std::string string_convert(const char16_t *const &);
std::string string_convert(const std::u16string &);
std::u16string string_convert(const char *const &);
std::u16string string_convert(const std::string &);

// C++ --> JS
JSString &string(const char *const &, const size_t &len);
JSString &string(const char *const &);
JSString &string(const std::string &);

// JS --> C++
std::string string(const JSString &);
std::string string(const JSString *const &);

std::string string(const JS::Value &);
std::string string(const jsid &);


inline std::string
string(const jsid &hid)
{
	return string(id(hid));
}

inline std::string
string(const JS::Value &s)
{
	return s.isString()? string(s.toString()) : std::string{};
}

inline std::string
string(const JSString *const &s)
{
	return s? string(*s) : std::string{};
}

inline std::string
string(const JSString &s)
{
	std::string ret(JS_GetStringEncodingLength(*cx, const_cast<JSString *>(&s)), char());
	JS_EncodeStringToBuffer(*cx, const_cast<JSString *>(&s), &ret.front(), ret.size());
	return ret;
}

inline JS::RootedString
string(const std::string &s,
       rooted_t)
{
	return { *cx, &string(s) };
}

inline JSString &
string(const std::string &s)
{
	return string(s.data(), s.size());
}

inline JSString &
string(const char *const &s)
{
	return string(s, strlen(s));
}

inline JSString &
string(const char *const &s,
       const size_t &len)
{
	const auto ret(JS_NewStringCopyN(*cx, s, len));
	if(unlikely(!ret))
		std::terminate();  //TODO: exception

	return *ret;
}

} // namespace js
} // namespace ircd
