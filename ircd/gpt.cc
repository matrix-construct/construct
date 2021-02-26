// Tensor Construct
//
// Copyright (C) Matrix Construct Developers, Authors & Contributors
// Copyright (C) 2016-2021 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

namespace ircd::gpt::vocab
{
	static u16 find_token(const u8x16) noexcept;
	static uint find_tokens(u16x16 &, const uint, const u8x16 (&)[16], const uint) noexcept;
	static u16 find_merge(const u8x16, const u8x16) noexcept;
	static u16 bpe_score(u16 (&)[16], const u8x16 (&)[16][2], const uint) noexcept;
	static uint bpe_merge(u8x16 (&)[16][2], u16 (&)[16], const uint, const u16) noexcept;
	static uint bpe_postpare(u8x16 (&)[16], const u8x16 (&)[16][2], const uint) noexcept;
	static uint bpe_prepare(u8x16 (&)[16][2], const u8x16) noexcept;
	static uint bpe_tokenize(u8x16 (&)[16], const u8x16) noexcept;
	static u64x2 pre_tokenize_split(u8x16 (&)[16], u32x16, u32x16, u32x16) noexcept;
	static u64x2 pre_tokenize(u8x16 (&)[16], const u8x16, const u8x16) noexcept;
	static u64x2 tokenize_block(u16x16 &, const u8x16, const u8x16) noexcept;
	static void init_tokens() noexcept;
	static void init_merges() noexcept;

	extern conf::item<std::string> tokens_path;
	extern conf::item<std::string> merges_path;
}

decltype(ircd::gpt::vocab::tokens)
ircd::gpt::vocab::tokens;

decltype(ircd::gpt::vocab::merges)
ircd::gpt::vocab::merges;

decltype(ircd::gpt::vocab::token)
ircd::gpt::vocab::token
alignas(64);

decltype(ircd::gpt::vocab::merge)
ircd::gpt::vocab::merge
alignas(64);

decltype(ircd::gpt::vocab::tokens_path)
ircd::gpt::vocab::tokens_path
{
	{
		{ "name",     "ircd.gpt.vocab.tokens.path" },
		{ "default",  string_view{}                },
	},
	init_tokens
};

decltype(ircd::gpt::vocab::merges_path)
ircd::gpt::vocab::merges_path
{
	{
		{ "name",     "ircd.gpt.vocab.merges.path" },
		{ "default",  string_view{}                },
	},
	init_merges
};

void
ircd::gpt::vocab::init_tokens()
noexcept
{
	if(!tokens_path)
		return;

	const ircd::fs::fd file
	{
		string_view{tokens_path}
	};

	const ircd::fs::map vocab_json
	{
		file, ircd::fs::map::opts{}
	};

	tokens = 0;
	for(const auto &[key, val] : json::object(vocab_json))
	{
		assert(tokens == lex_cast<uint16_t>(val));
		json::unescape(token[tokens++], key);
	}
}

void
ircd::gpt::vocab::init_merges()
noexcept
{
	if(!merges_path)
		return;

	const ircd::fs::fd file
	{
		string_view{merges_path}
	};

	const ircd::fs::map merges_txt
	{
		file, ircd::fs::map::opts{}
	};

	merges = 0;
	ircd::tokens(split(merges_txt, '\n').second, '\n', []
	(const string_view &line)
	{
		const auto &[a, b]
		{
			split(line, ' ')
		};

		copy(merge[merges][0], a);
		copy(merge[merges][1], b);
		++merges;
	});
}

//
// detokenize
//

ircd::string_view
ircd::gpt::vocab::detokenize(const mutable_buffer &out,
                             const vector_view<const u16> &in)
noexcept
{
	mutable_buffer buf(out);
	for(const u16 &token : in)
		consume(buf, copy(buf, const_buffer(vocab::token[token], simd::strlen(vocab::token[token]))));

	return string_view
	{
		data(out), data(buf)
	};
}

//
// tokenize
//

ircd::vector_view<ircd::u16>
ircd::gpt::vocab::tokenize(const vector_view<u16> &out,
                           const string_view &in)
noexcept
{
	using input_t = u8x16;
	using block_t = u16x16;

	assert(out.size() >= simd::lanes<block_t>());
	const u64x2 max
	{
		out.size(), in.size(),
	};

	const auto block
	{
		reinterpret_cast<block_t *>(out.data())
	};

	const auto consumed
	{
		simd::tokens<input_t, block_t>(block, in.data(), max, gpt::vocab::tokenize_block)
	};

	assert(consumed[0] <= out.size());
	return vector_view<u16>
	(
		out.data(), consumed[0]
	);
}

ircd::u64x2
ircd::gpt::vocab::tokenize_block(u16x16 &token,
                                 const u8x16 in,
                                 const u8x16 in_mask)
noexcept
{
	u8x16 pre_token[16];
	const auto &[pre_tokens, consumed]
	{
		pre_tokenize(pre_token, in, in_mask)
	};

	uint tokens(0);
	for(uint i(0); i < pre_tokens; ++i)
	{
		u8x16 str[16];
		const uint strs
		{
			bpe_tokenize(str, pre_token[i])
		};

		const uint addl_tokens
		{
			find_tokens(token, tokens, str, strs)
		};

		tokens += addl_tokens;
	}

	return u64x2
	{
		tokens, consumed
	};
}

//
// pre-tokenizer
//

/// Pre-tokenizationis formalized by the regular expression:
///
/// 's|'t|'re|'ve|'m|'ll|'d| ?\p{L}+| ?\p{N}+| ?[^\s\p{L}\p{N}]+|\s+(?!\S)|\s+
///
/// The return value in [0] indicates the number of tokens populated in the
/// array; the value in [1] indicates the bytes consumed from the input.
///
ircd::u64x2
ircd::gpt::vocab::pre_tokenize(u8x16 (&token)[16],
                               const u8x16 in,
                               const u8x16 in_mask)
noexcept
{
	const u8x16 is_ascii_ctrl
	{
		in < 0x20
	};

	const u8x16 is_ascii_space
	{
		in == ' '
	};

	const u8x16 is_ascii_number
	{
		in >= '0' && in <= '9'
	};

	const u8x16 is_ascii_letter
	{
		(in >= 'a' && in <= 'z') || (in >= 'A' && in <= 'Z')
	};

	const u8x16 ascii_identified
	{
		is_ascii_space | is_ascii_number | is_ascii_letter
	};

	const u8x16 maybe_notascii
	{
		~ascii_identified & in_mask
	};

	const u32x16 ch
	{
		utf8::decode(in)
	};

	const u32x16 uc_cat
	{
		icu::category(ch & (lane_cast<u32x16>(maybe_notascii) != 0))
	};

	const u32x16 is_L
	{0
		| ((uc_cat & 0x0000003eU) != 0)
		| (lane_cast<u32x16>(is_ascii_letter) != 0)
	};

	const u32x16 is_N
	{0
		| ((uc_cat & 0x00000e00U) != 0)
		| (lane_cast<u32x16>(is_ascii_number) != 0)
	};

	const u32x16 is_Z
	{0
		| ((uc_cat & 0x00007000U) != 0)
		| (lane_cast<u32x16>(is_ascii_space) != 0)
	};

	const u32x16 is_trail
	{0
		| (is_L & shl<32>(is_L))
		| (is_N & shl<32>(is_N))
		| (is_Z & shl<32>(is_Z))
	};

	const u32x16 fat_mask
	{
		lane_cast<u32x16>(in_mask) != 0
	};

	const u32x16 is_head
	{
		~is_trail & fat_mask
	};

	// mask if token is preceded by a space
	const u32x16 leading_space
	{
		is_head & shl<32>(is_Z)
	};

	// zero or one preceding space becomes prefixed to the next token
	const u32x16 tok_head
	{0
		| (is_head & ~leading_space)
		| shr<32>(leading_space)
	};

	const u32x16 tok_trail
	{
		~tok_head
	};

	const u32x16 tok_mask
	{
		tok_trail
	};

	const u64x2 ret
	{
		pre_tokenize_split(token, ch, fat_mask, tok_mask)
	};

	return ret;
}

/// Split single vector of UTF-32 codepoints into vectors of UTF-8 strings for
/// each token determined by the input masks. Returns the number of tokens in
/// [0] and the number of codepoints consumed in [1].
ircd::u64x2
ircd::gpt::vocab::pre_tokenize_split(u8x16 (&token)[16],
                                     u32x16 ch,
                                     u32x16 ch_mask,
                                     u32x16 tok_mask)
noexcept
{
	const u32x16 lane0_mask
	{
		-1U
	};

	u64x2 ret
	{
		0, 0
	};

	for(uint i(0); ret[0] == i && ret[1] < 16; ++i)
	{
		// Create a mask from all non-leading characters of input tokens with
		// a mask of just the leading character of the first token. To be sure
		// extra characters are not included we rinse it with the ch_mask.
		const u32x16 cover_mask
		{
			(lane0_mask | tok_mask) & ch_mask
		};

		// Get the length of the first token from the cover.
		const u64 len
		{
			std::min(simd::lzcnt(~cover_mask) / 32, 16U)
		};

		// When the first token is too large, we truncate that token here and
		// return, effectively splitting the token into multiple. If the token
		// after the first is too large (input potentially spans into the next
		// block), we kick it to the next iteration entirely.
		const bool skip
		{
			len >= 16 - ret[1] && ret[0]
		};

		// Generate utf-8 codepoints
		const u32x16 ch8
		{
			utf8::encode(ch & cover_mask)
		};

		// Pack the utf-8 codepoints into the result token
		token[i] = {0};
		for(uint j(0); j < 16; ++j)
			token[i][j] = ch8[j];

		// Shift the token off the input to consume the next.
		for(uint j(0); j < len; ++j)
		{
			ch = shr<32>(ch);
			ch_mask = shr<32>(ch_mask);
			tok_mask = shr<32>(tok_mask);
		}

		ret[0] += bool(len) && !skip;
		ret[1] += len & boolmask<u64>(!skip);
	}

	return ret;
}

//
// byte-pair encoding
//

[[gnu::noinline]]
uint
ircd::gpt::vocab::bpe_tokenize(u8x16 (&str)[16],
                               const u8x16 pre_token)
noexcept
{
	u8x16 pair[16][2];
	auto pairs
	{
		bpe_prepare(pair, pre_token)
	};

	u16 score[16] {0};
	for(uint j(0); j < 16 && pairs > 1; ++j)
	{
		const auto best_score
		{
			bpe_score(score, pair, pairs)
		};

		if(best_score >= u16(-1))
			break;

		const auto merges
		{
			bpe_merge(pair, score, pairs, best_score)
		};

		pairs -= merges;
		if(!merges)
			break;
	}

	const uint strs
	{
		bpe_postpare(str, pair, pairs)
	};

	return strs;
}

uint
ircd::gpt::vocab::bpe_prepare(u8x16 (&out)[16][2],
                              const u8x16 in)
noexcept
{
	uint di, si;
	for(di = 0, si = 0; si < 16 && di < 16; si += 2, di += 2)
	{
		out[di][0] = {0};
		out[di][1] = {0};
		if(!in[si] || !in[si + 1])
			break;

		//TODO: XXX
		if(!si && in[si] == ' ')
		{
			out[di][0][0] = 0xc4;
			out[di][0][1] = 0xa0;
			out[di][1][0] = in[si + 1];
			continue;
		}

		out[di][0][0] = in[si];
		out[di][1][0] = in[si + 1];
	}

	for(di = 1, si = 1; si < 16 && di < 16; si += 2, di += 2)
	{
		out[di][0] = {0};
		out[di][1] = {0};
		if(!in[si] || !in[si + 1])
			break;

		out[di][0][0] = in[si];
		out[di][1][0] = in[si + 1];
	}

	return di;
}

uint
ircd::gpt::vocab::bpe_postpare(u8x16 (&out)[16],
                               const u8x16 (&in)[16][2],
                               const uint num)
noexcept
{
	uint ret(0);
	for(uint j(0); j < num; ++j)
		if(simd::strlen(in[j][0]))
			out[ret++] = in[j][0];

	if(likely(num))
		if(simd::strlen(in[num - 1][1]))
			out[ret++] = in[num - 1][1];

	return ret;
}

uint
ircd::gpt::vocab::bpe_merge(u8x16 (&pair)[16][2],
                            u16 (&score)[16],
                            const uint num,
                            const u16 best_score)
noexcept
{

	uint ret(0);
	for(uint i(0); i < num - ret; ++i)
	{
		if(score[i] != best_score)
			continue;

		pair[i][0] = simd::strcat(pair[i][0], pair[i][1]);
		score[i] = 0;

		if(i > 0)
		{
			pair[i - 1][1] = simd::strcat(pair[i - 1][1], pair[i][1]);
			score[i - 1] = 0;
		}

		if(i < 15)
			pair[i][1] = pair[i + 1][1];

		for(uint j(i + 1); j + 1 < num; ++j)
		{
			pair[j][0] = pair[j + 1][0];
			pair[j][1] = pair[j + 1][1];
			score[j] = score[j + 1];
		}

		++ret;
	}

	return ret;
}

ircd::u16
ircd::gpt::vocab::bpe_score(u16 (&score)[16],
                            const u8x16 (&pair)[16][2],
                            const uint num)
noexcept
{
	uint best(-1U), is_min;
	for(uint i(0); i < num; i++)
	{
		// Only find the merge if the score is set to zero.
		if(!score[i])
			score[i] = find_merge(pair[i][0], pair[i][1]);

		// If the score is set to -1 this index is inactive or wasn't a
		// valid pair.
		is_min = boolmask<uint>(score[i] != u16(-1));
		is_min &= boolmask<uint>(score[i] < best);
		best = (is_min & score[i]) | (~is_min & best);
	}

	return best;
}

//
// queries
//

uint
ircd::gpt::vocab::find_tokens(u16x16 &token,
                              const uint tokens,
                              const u8x16 (&str)[16],
                              const uint strs)
noexcept
{
	uint ret(0);
	for(; tokens + ret < 16 && ret < strs; ++ret)
	{
		const auto val
		{
			find_token(str[ret])
		};

		const bool found
		{
			val != u16(-1)
		};

		assert(found);
		token[tokens + ret] = val;
	}

	return ret;
}

ircd::u16
ircd::gpt::vocab::find_token(const u8x16 string)
noexcept
{
	const auto *const __restrict__ token
	{
		reinterpret_cast<const u8x16 *>(vocab::token)
	};

	for(uint i(0); i < tokens; ++i)
		if(simd::streq(string, token[i]))
			return i;

	return u16(-1U);
}

ircd::u16
ircd::gpt::vocab::find_merge(const u8x16 a,
                             const u8x16 b)
noexcept
{
	const auto &__restrict__ merge
	{
		reinterpret_cast<const u8x16 (&)[65536][2]>(vocab::merge)
	};

	for(uint i(0); i < merges; ++i)
	{
		if(likely(!simd::streq(a, merge[i][0])))
			continue;

		if(likely(!simd::streq(b, merge[i][1])))
			continue;

		return i;
	}

	return u16(-1U);
}
