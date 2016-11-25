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
#define HAVE_IRCD_JS_JSON_H

namespace ircd {
namespace js   {
namespace json {

using closure = std::function<bool (const char16_t *, uint32_t)>;
void stringify(const value::handle_mutable &, const JS::HandleObject &fmtr, const value::handle &sp, const closure &);
void stringify(const value::handle_mutable &, const closure &);

std::u16string stringify(const value::handle_mutable &, const JS::HandleObject &fmtr, const value::handle &sp);
std::u16string stringify(const value::handle_mutable &, const bool &pretty = false);
std::u16string stringify(value &, const bool &pretty = false);
std::u16string stringify(const value &, const bool &pretty = false);

value parse(const char16_t *const &, const size_t &);
value parse(const std::u16string &);
value parse(const std::string &);
value parse(const char *const &);
value parse(const string &);

} // namespace json
} // namespace js
} // namespace ircd
