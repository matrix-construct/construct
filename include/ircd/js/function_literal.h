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
#define HAVE_IRCD_JS_FUNCTION_LITERAL_H

namespace ircd {
namespace js   {

class function::literal
:public root<JSFunction *>
{
	const char *name;
	const char *text;
	std::vector<const char *> prototype;

  public:
	literal(const char *const &name,
	        const std::initializer_list<const char *> &prototype,
	        const char *const &text);

	literal(literal &&) noexcept;
	literal(const literal &) = delete;
};

// Compile a C++ string literal as an actual JavaScript function.
function::literal operator ""_function(const char *const text, const size_t len);

} // namespace js
} // namespace ircd
