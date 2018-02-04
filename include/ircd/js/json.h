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
#define HAVE_IRCD_JS_JSON_H

namespace ircd {
namespace js   {
namespace json {

using closure = std::function<bool (const char16_t *, uint32_t)>;
void stringify(const value::handle_mutable &, const JS::HandleObject &fmtr, const value &sp, const closure &);
void stringify(const value::handle_mutable &, const closure &);

std::u16string stringify(const value::handle_mutable &, const JS::HandleObject &fmtr, const value &sp);
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
