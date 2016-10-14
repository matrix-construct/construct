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
#define HAVE_IRCD_JS_STRING

namespace ircd {
namespace js   {

JS::RootedString string(context &, const std::string &);
std::string string(context &, JSString &);
std::string string(context &, const JS::HandleValue &);


inline std::string
string(context &c,
       const JS::HandleValue &s)
{
	const auto jstr(JS::ToString(c, s));
	return jstr? string(c, *jstr) : std::string{};
}

inline std::string
string(context &c,
       JSString &s)
{
	std::string ret(JS_GetStringEncodingLength(c, &s), char());
	JS_EncodeStringToBuffer(c, &s, &ret.front(), ret.size());
	return ret;
}

inline JS::RootedString
string(context &c,
       const std::string &s)
{
	return { c, JS_NewStringCopyN(c, s.data(), s.size()) };
}

} // namespace js
} // namespace ircd
