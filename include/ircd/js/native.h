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
	ircd::locale::char16::conv(s, buf.get(), len + 1);
	return buf;
}
