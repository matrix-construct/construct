// Matrix Construct
//
// Copyright (C) Matrix Construct Developers, Authors & Contributors
// Copyright (C) 2016-2018 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

//
// TODO: This file will be merged with string.h.
// Don't confuse it with SpiderMonkey terminology for "JSNative" functions
//

#pragma once
#define HAVE_IRCD_JS_NATIVE_H

namespace ircd {
namespace js   {

extern JSStringFinalizer native_external_delete; // calls `delete[]` on char16_t[]
extern JSStringFinalizer native_external_static; // no-op for static/literal or self-managed

std::unique_ptr<char16_t[]> native_external_copy(const char *const &s, const size_t &len);
std::unique_ptr<char16_t[]> native_external_copy(const char *const &s);
std::unique_ptr<char16_t[]> native_external_copy(const std::string &s);

size_t native(const JSString *const &, char *const &buf, const size_t &max);
size_t native_size(const JSString *const &);
std::string native(const JSString *const &);

} // namespace js
} // namespace ircd

inline std::unique_ptr<char16_t[]>
ircd::js::native_external_copy(const std::string &s)
{
	return native_external_copy(s.data(), s.size());
}

inline std::unique_ptr<char16_t[]>
ircd::js::native_external_copy(const char *const &s)
{
	return native_external_copy(s, strlen(s));
}

inline std::unique_ptr<char16_t[]>
ircd::js::native_external_copy(const char *const &s,
                               const size_t &len)
{
	auto buf(std::make_unique<char16_t[]>(len + 1));
	ircd::locale::char16::conv(s, len, buf.get(), len + 1);
	return buf;
}
