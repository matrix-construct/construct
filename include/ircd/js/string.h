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

// C++ --> JS
JSString &string(context &, const std::string &);
JS::RootedString string(context &, const std::string &, rooted_t);

// JS --> C++
std::string string(context &, const JSString &);
std::string string(context &, const JSString *const &);

std::string string(context &, const JS::Value &);
std::string string(context &, const jsid &);


inline std::string
string(context &c,
       const jsid &hid)
{
	return string(c, id(c, hid));
}

inline std::string
string(context &c,
       const JS::Value &s)
{
	return s.isString()? string(c, s.toString()) : std::string{};
}

inline std::string
string(context &c,
       const JSString *const &s)
{
	return s? string(c, *s) : std::string{};
}

inline std::string
string(context &c,
       const JSString &s)
{
	std::string ret(JS_GetStringEncodingLength(c, const_cast<JSString *>(&s)), char());
	JS_EncodeStringToBuffer(c, const_cast<JSString *>(&s), &ret.front(), ret.size());
	return ret;
}

inline JS::RootedString
string(context &c,
       const std::string &s,
       rooted_t)
{
	return { c, &string(c, s) };
}

inline JSString &
string(context &c,
       const std::string &s)
{
	const auto ret(JS_NewStringCopyN(c, s.data(), s.size()));
	if(unlikely(!ret))
		std::terminate();  //TODO: exception

	return *ret;
}

} // namespace js
} // namespace ircd
