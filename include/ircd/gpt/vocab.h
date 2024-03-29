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
#define HAVE_IRCD_GPT_VOCAB_H

/// Vocabulary Tokenization & Encoding
///
namespace ircd::gpt::vocab
{
	IRCD_EXCEPTION(gpt::error, error)

	// Actual number of tokens and merges stored in following lists.
	extern size_t
	tokens,
	merges;

	// Lists of tokens and merges. Values are strings up to length maxlen which
	// are null terminated if shorter.
	extern char
	token [65536][16],
	merge [65536][2][16];

	// Paths to the files containing token and merge datas.
	extern conf::item<std::string>
	tokens_path,
	merges_path;

	// Tokenize UTF-8 input string of any length into proper token values,
	vector_view<u16> tokenize(const vector_view<u16> &out, const string_view &in) noexcept;

	// Tokenize one token. The buffer is advanced consuming one token per call.
	u16 tokenize(const_buffer &) noexcept;

	// Tokenize one token. Error thrown if input is not exactly one token.
	u16 tokenize(const string_view &in, const bool prefix_space = false);

	// Return the number of tokens which would be output by a tokenize().
	size_t count(const string_view &in) noexcept;

	// Decode token values to build output text string.
	string_view detokenize(const mutable_buffer &out, const vector_view<const u16> &in) noexcept;

	// Return the length of the string which would be output by a detokenize().
	size_t count(const vector_view<const u16> &in) noexcept;

	// Other tools
	string_view debug(const mutable_buffer &buf, const u16 token, const uint fmt_msk = -1U);
}
