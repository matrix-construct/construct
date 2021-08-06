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
	static u16 find_token(const u8x16);
	static u16 find_merge(const u8x16, const u8x16);
	static u16 bpe_score(u16 (&)[16], const u8x16 (&)[16][2], const uint);
	static uint bpe_merge(u8x16 (&)[16][2], u16 (&)[16], const uint, const u16);
	static uint bpe_postpare(u8x16 (&)[16], const u8x16 (&)[16][2], const uint);
	static uint bpe_prepare(u8x16 (&)[16][2], const u8x16);
	static uint bpe_tokenize(u8x16 (&)[16], const u8x16);
	static std::array<u32x16, 3> pre_tokenize_split(const u8x16, const u8x16);
	static u64x2 pre_tokenize(u8x16 (&)[16], const u8x16, const u8x16);
	static u64x2 unk_tokenize(u16x16 &, const u8x16, u64);
	static u64x2 tokenize_block(u16x16 &, const u8x16, const u8x16) noexcept;
	static void init_tokens(), init_merges();

	[[gnu::visibility("internal")]]
	extern const char32_t charset[256];
}

/// Remapping of single byte characters (Control (C0) and Basic Latin (ASCII)).
decltype(ircd::gpt::vocab::charset)
ircd::gpt::vocab::charset
alignas(64)
{
	U'Ā',   U'ā',   U'Ă',   U'ă',   U'Ą',   U'ą',   U'Ć',   U'ć',   // [0x07]
	U'Ĉ',   U'ĉ',   U'Ċ',   U'ċ',   U'Č',   U'č',   U'Ď',   U'ď',   // [0x0F]
	U'Đ',   U'đ',   U'Ē',   U'ē',   U'Ĕ',   U'ĕ',   U'Ė',   U'ė',   // [0x17]
	U'Ę',   U'ę',   U'Ě',   U'ě',   U'Ĝ',   U'ĝ',   U'Ğ',   U'ğ',   // [0x1F]
	U'Ġ',   U'!',   U'"',   U'#',   U'$',   U'%',   U'&',   U'\'',  // [0x27]
	U'(',   U')',   U'*',   U'+',   U',',   U'-',   U'.',   U'/',   // [0x2F]
	U'0',   U'1',   U'2',   U'3',   U'4',   U'5',   U'6',   U'7',   // [0x37]
	U'8',   U'9',   U':',   U';',   U'<',   U'=',   U'>',   U'?',   // [0x3F]
	U'@',   U'A',   U'B',   U'C',   U'D',   U'E',   U'F',   U'G',   // [0x47]
	U'H',   U'I',   U'J',   U'K',   U'L',   U'M',   U'N',   U'O',   // [0x4F]
	U'P',   U'Q',   U'R',   U'S',   U'T',   U'U',   U'V',   U'W',   // [0x57]
	U'X',   U'Y',   U'Z',   U'[',   U'\\',  U']',   U'^',   U'_',   // [0x5F]
	U'`',   U'a',   U'b',   U'c',   U'd',   U'e',   U'f',   U'g',   // [0x67]
	U'h',   U'i',   U'j',   U'k',   U'l',   U'm',   U'n',   U'o',   // [0x6F]
	U'p',   U'q',   U'r',   U's',   U't',   U'u',   U'v',   U'w',   // [0x77]
	U'x',   U'y',   U'z',   U'{',   U'|',   U'}',   U'~',   U'ġ',   // [0x7F]
	U'Ģ',   U'ģ',   U'Ĥ',   U'ĥ',   U'Ħ',   U'ħ',   U'Ĩ',   U'ĩ',   // [0x87]
	U'Ī',   U'ī',   U'Ĭ',   U'ĭ',   U'Į',   U'į',   U'İ',   U'ı',   // [0x8F]
	U'Ĳ',   U'ĳ',   U'Ĵ',   U'ĵ',   U'Ķ',   U'ķ',   U'ĸ',   U'Ĺ',   // [0x97]
	U'ĺ',   U'Ļ',   U'ļ',   U'Ľ',   U'ľ',   U'Ŀ',   U'ŀ',   U'Ł',   // [0x9F]
	U'ł',   U'¡',   U'¢',   U'£',   U'¤',   U'¥',   U'¦',   U'§',   // [0xA7]
	U'¨',   U'©',   U'ª',   U'«',   U'¬',   U'Ń',   U'®',   U'¯',   // [0xAF]
	U'°',   U'±',   U'²',   U'³',   U'´',   U'µ',   U'¶',   U'·',   // [0xB7]
	U'¸',   U'¹',   U'º',   U'»',   U'¼',   U'½',   U'¾',   U'¿',   // [0xBF]
	U'À',   U'Á',   U'Â',   U'Ã',   U'Ä',   U'Å',   U'Æ',   U'Ç',   // [0xC7]
	U'È',   U'É',   U'Ê',   U'Ë',   U'Ì',   U'Í',   U'Î',   U'Ï',   // [0xCF]
	U'Ð',   U'Ñ',   U'Ò',   U'Ó',   U'Ô',   U'Õ',   U'Ö',   U'×',   // [0xD7]
	U'Ø',   U'Ù',   U'Ú',   U'Û',   U'Ü',   U'Ý',   U'Þ',   U'ß',   // [0xDF]
	U'à',   U'á',   U'â',   U'ã',   U'ä',   U'å',   U'æ',   U'ç',   // [0xE7]
	U'è',   U'é',   U'ê',   U'ë',   U'ì',   U'í',   U'î',   U'ï',   // [0xEF]
	U'ð',   U'ñ',   U'ò',   U'ó',   U'ô',   U'õ',   U'ö',   U'÷',   // [0xF7]
	U'ø',   U'ù',   U'ú',   U'û',   U'ü',   U'ý',   U'þ',   U'ÿ',   // [0xFF]
};

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

		auto &buf
		{
			token[tokens++]
		};

		const auto unescaped
		{
			json::unescape(buf, key)
		};

		for(size_t i(size(unescaped)); i < 16; ++i)
			buf[i] = 0;
	}
}

void
ircd::gpt::vocab::init_merges()
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

ircd::string_view
ircd::gpt::vocab::debug(const mutable_buffer &out,
                        const u16 idx)
{
	const auto *const token
	{
		reinterpret_cast<const u8x16 *>(vocab::token)
	};

	thread_local char strbuf[2][512];
	return string_view{fmt::sprintf
	{
		out, "%5u  %s  [%32s]",
		idx,
		simd::print_mem(strbuf[0], token[idx]),
		simd::print_chr(strbuf[1], token[idx]),
	}};
}

//
// detokenize
//

ircd::string_view
ircd::gpt::vocab::detokenize(const mutable_buffer &out,
                             const vector_view<const u16> &in)
{
	size_t off(0);
	for(const u16 &id : in)
	{
		const auto &token
		{
			vocab::token[id]
		};

		const string_view text
		{
			token, strnlen(token, 16)
		};

		string_view dest
		{
			data(out + off), copy(out + off, text)
		};

		dest = replace(out + off, dest, "Ġ"_sv, " "_sv);
		dest = replace(out + off, dest, "Ċ"_sv, "\n"_sv);
		off += size(dest);
	}

	assert(off <= size(out));
	return string_view
	{
		data(out), off
	};
}

//
// tokenize
//

ircd::vector_view<ircd::u16>
ircd::gpt::vocab::tokenize(const vector_view<u16> &out,
                           const string_view &in)
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
	assert(consumed[0] <= consumed[1]);
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
	const auto [pre_tokens, consumed]
	{
		pre_tokenize(pre_token, in, in_mask)
	};

	u64x2 ret
	{
		0, consumed
	};

	assert(consumed);
	for(uint i(0); i < pre_tokens && ret[0] < 16; ++i)
	{
		// one token in hand is worth two in the bpe
		if(likely((token[ret[0]] = find_token(pre_token[i])) != u16(-1)))
		{
			++ret[0];
			continue;
		}

		u8x16 str[16];
		const uint strs
		{
			bpe_tokenize(str, pre_token[i])
		};

		for(uint j(0); j < strs && ret[0] < 16; ++j)
		{
			if(likely((token[ret[0]] = find_token(str[j])) != u16(-1)))
			{
				++ret[0];
				continue;
			}

			ret += unk_tokenize(token, str[j], ret[0]);
		}
	}

	assert(ret[1]);
	return ret;
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

/// Split single vector of UTF-32 codepoints into vectors of UTF-8 strings for
/// each token determined by the input masks. Returns the number of tokens in
/// [0] and the number of codepoints consumed in [1].
ircd::u64x2
ircd::gpt::vocab::pre_tokenize(u8x16 (&token)[16],
                               const u8x16 in,
                               const u8x16 in_mask)
{
	auto [ch, ch_mask, tok_mask]
	{
		pre_tokenize_split(in, in_mask)
	};

	// Replace single-byte codepoints from the LUT.
	u32x16 rch;
	for(uint i(0); i < 16; ++i)
		rch[i] = ch[i] > 0xFF?
			ch[i]: charset[ch[i]];

	u64x2 ret {0, 0};
	for(uint i(0); ret[0] < 16 && ret[1] < 16; ++i)
	{
		static const u32x16 lane0_mask
		{
			-1U, 0
		};

		// Create a mask from all non-leading characters of input tokens with
		// a mask of just the leading character of the first token. To be sure
		// extra characters are not included we rinse it with the ch_mask.
		const u32x16 cover_mask
		(
			(lane0_mask | tok_mask) & ch_mask
		);

		// Get the number of codepoints of the first token from the cover.
		const auto cp_num
		{
			std::min(simd::lzcnt(~cover_mask | ~ch_mask) / 32UL, 16UL)
		};

		// Input codepoint lengths
		const u32x16 cp_len
		(
			utf8::length(ch) & cover_mask
		);

		// Output codepoint lengths
		const u32x16 rcp_len
		(
			utf8::length(rch) & cover_mask
		);

		// Generate utf-8 codepoints
		const u8x64 rch8
		(
			utf8::encode_sparse(rch & cover_mask)
		);

		u32x16 idx;
		uint off(0), len(0);
		for(uint j(0); j < cp_num; ++j)
			idx[j] = off,
			off += rcp_len[j],
			len += cp_len[j];

		// One token over the line...
		if(ret[1] + off >= 16 && i > 0)
			break;

		// We have to return the proper number of bytes for what was truncated
		// from the input, but the truncation is determined after a transform
		// which may have a different size; this has to be offset back now.
		if(ret[1] + off > 16)
			len -= (ret[1] + off - 1) - 16;

		// Pack the utf-8 codepoints into the result token
		token[i] = u8x16{0};
		for(uint j(0); j < cp_num; ++j)
			for(uint k(0); k < rcp_len[j] && idx[j] + k < 16; ++k)
				token[i][idx[j] + k] = rch8[j * 4 + k];

		// Shift the token off the input to consume the next.
		for(uint j(0); j < cp_num; ++j)
		{
			ch = shr<32>(ch);
			rch = shr<32>(rch);
			ch_mask = shr<32>(ch_mask);
			tok_mask = shr<32>(tok_mask);
		}

		ret[0] += 1;
		ret[1] += len;
		assert(len <= 16);
	}

	return ret;
}

std::array<ircd::u32x16, 3>
ircd::gpt::vocab::pre_tokenize_split(const u8x16 in,
                                     const u8x16 in_mask)
{
	const u8x16 is_ascii_ctrl
	(
		in < 0x20
	);

	const u8x16 is_ascii_space
	(
		in == ' '
	);

	const u8x16 is_ascii_number
	(
		in >= '0' && in <= '9'
	);

	const u8x16 is_ascii_letter
	(
		(in >= 'a' && in <= 'z') ||
		(in >= 'A' && in <= 'Z')
	);

	const u8x16 is_ascii_punct
	(
		(in >= '!' && in <= '/') ||
		(in >= ':' && in <= '@') ||
		(in >= '[' && in <= '`') ||
		(in >= '{' && in <= '~')
	);

	const u8x16 ascii_categorized
	(0
		| is_ascii_ctrl
		| is_ascii_space
		| is_ascii_punct
		| is_ascii_letter
		| is_ascii_number
	);

	const u8x16 maybe_notascii
	(
		~ascii_categorized & in_mask
	);

	const u32x16 ch
	(
		utf8::decode(in)
	);

	const u32x16 ch_mask
	(
		lane_cast<u32x16>(in_mask) != 0
	);

	const u32x16 uc_cat
	(
		icu::category(ch & (lane_cast<u32x16>(maybe_notascii) != 0))
	);

	const u32x16 is_L
	(0
		| ((uc_cat & 0x0000003eU) != 0)
		| (lane_cast<u32x16>(is_ascii_letter) != 0)
	);

	const u32x16 is_N
	(0
		| ((uc_cat & 0x00000e00U) != 0)
		| (lane_cast<u32x16>(is_ascii_number) != 0)
	);

	const u32x16 is_Z
	(0
		| ((uc_cat & 0x00007000U) != 0)
		| (lane_cast<u32x16>(is_ascii_space) != 0)
	);

	const u32x16 is_C0
	(0
		| (lane_cast<u32x16>(is_ascii_ctrl) != 0)
	);

	const u32x16 is_punct
	(0
		| (lane_cast<u32x16>(is_ascii_punct) != 0)
	);

	// Decide characters which do not start a new token based on the
	// preceding character.
	const u32x16 is_trail
	(0
		| (is_L & shl<32>(is_L))
		| (is_N & shl<32>(is_N))
		| (is_Z & shl<32>(is_Z))
		| (is_L & shl<32>(is_punct))
		| (is_punct & shl<32>(is_punct))
	);

	// Decide characters which may start a token.
	const u32x16 is_head
	(
		(~is_trail | is_C0) & ch_mask
	);

	// Decide if candidate token is preceded by a space.
	const u32x16 leading_space
	(
		is_head & shl<32>(is_Z)
	);

	// Mask if next char is also the same char.
	const u32x16 is_rep
	(
		is_head & (shl<32>(ch) == ch)
	);

	// Decide the starting character of each token.
	const u32x16 tok_head
	(0
		| (is_head & ~leading_space & ~is_rep)
		| shr<32>(leading_space)
	);

	const u32x16 tok_trail
	(
		~tok_head
	);

	const u32x16 tok_mask
	(
		tok_trail
	);

	return
	{
		ch,
		ch_mask,
		tok_mask
	};
}

//
// post-tokenizer
//

[[gnu::noinline]]
ircd::u64x2
ircd::gpt::vocab::unk_tokenize(u16x16 &token,
                               const u8x16 str,
                               const u64 num)
{
	const auto len
	{
		simd::strlen(str)
	};

	u64 tokens(0), consumed(0);
	while(consumed < len && num + tokens < 16)
	{
		uint slen(0);
		for(uint i(0); i < len - consumed; ++i)
		{
			u8x16 s(str);
			for(uint j(0); j < consumed; ++j)
				s = shr<8>(s);

			for(uint j(i + 1); j < 16; ++j)
				s[j] = 0;

			u16 tok;
			if((tok = find_token(s)) == u16(-1))
				continue;

			token[num + tokens] = tok;
			slen = simd::strlen(s);
		}

		// Last possible branch; token is bytewise identity.
		if(!slen)
			token[num + tokens] = str[consumed];

		assert(slen < 16);
		consumed += std::max(slen, 1U);
		tokens += 1U;
	}

	assert(len >= consumed);
	assert(num + tokens <= 16);
	const auto overflow{len - consumed};
	assert(overflow == 0 || num + tokens == 16);
	assert(consumed > 0 || tokens == 0);
	assert(tokens > 0 || len == 0);
	return u64x2
	{
		// return number of tokens created only; the caller already counted
		// the length of str as consumed input.
		tokens, 0
	};
}

//
// byte-pair encoding
//

[[gnu::noinline]]
uint
ircd::gpt::vocab::bpe_tokenize(u8x16 (&str)[16],
                               const u8x16 pre_token)
{
	if(simd::strlen(pre_token) < 2)
	{
		str[0] = pre_token;
		return 1;
	}

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
{
	const auto len
	{
		simd::strlen(in)
	};

	const u32x16 cplen
	(
		utf8::length(utf8::decode(in))
	);

	u32x16 idx;
	for(uint i(0), off(0); i < 16; off += cplen[i++])
		idx[i] = off;

	uint ret(0);
	for(uint phase(0); phase < 2; ++phase)
		for(uint i(phase); i < 16; i += 2, ++ret)
		{
			if(idx[i] >= 16 || !in[idx[i]])
				break;

			out[i][0] = u8x16{0};
			out[i][1] = u8x16{0};
			for(uint k(0); k < 2; ++k)
				for(uint j(0); j < cplen[i + k] && idx[i + k] + j < 16; ++j)
					out[i][k][j] = in[idx[i + k] + j];
		}

	return ret;
}

uint
ircd::gpt::vocab::bpe_postpare(u8x16 (&out)[16],
                               const u8x16 (&in)[16][2],
                               const uint num)
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

ircd::u16
ircd::gpt::vocab::find_token(const u8x16 string)
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
