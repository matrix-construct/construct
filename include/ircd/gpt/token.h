// Matrix Construct
//
// Copyright (C) Matrix Construct Developers, Authors & Contributors
// Copyright (C) 2016-2021 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

#pragma once
#define HAVE_IRCD_GPT_TOKEN_H

namespace ircd::gpt
{
	struct token;
}

/// Token is just a 16-bit index into the vocabulary. This lightweight wrapper
/// convenience constructs from a string lookup or from a u16 directly.
class ircd::gpt::token
{
	uint16_t val;

  public:
	operator const uint16_t &() const;
	operator uint16_t &();

	operator string_view() const;

	token(const_buffer &buf) noexcept;
	token(const string_view &, const bool prefix_space = false);
	token(const uint16_t &) noexcept;
};

static_assert(sizeof(ircd::gpt::token) == sizeof(uint16_t));
static_assert(std::is_standard_layout<ircd::gpt::token>::value);

/// Direct construction; no lookup
inline
ircd::gpt::token::token(const uint16_t &val)
noexcept
:val{val}
{}

/// Must resolve to one token or error thrown.
/// prefix_space=true internally prepends space for potentially better token.
inline
ircd::gpt::token::token(const string_view &str,
                        const bool prefix_space)
:val{vocab::tokenize(str, prefix_space)}
{}

/// Consumes input for one token off front of buf
inline
ircd::gpt::token::token(const_buffer &buf)
noexcept
:val{vocab::tokenize(buf)}
{}

inline ircd::gpt::token::operator
string_view()
const
{
	return vocab::token[val];
}

inline ircd::gpt::token::operator
uint16_t &()
{
	return val;
}

inline ircd::gpt::token::operator
const uint16_t &()
const
{
	return val;
}
