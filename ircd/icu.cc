// The Construct
//
// Copyright (C) The Construct Developers, Authors & Contributors
// Copyright (C) 2016-2020 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

#if __has_include(<unicode/uversion.h>)
	#include <unicode/uversion.h>
#endif

#if __has_include(<unicode/uchar.h>)
	#include <unicode/uchar.h>
#endif

decltype(ircd::icu::version_api)
ircd::icu::version_api
{
	"icu", info::versions::API, 0,
	#if __has_include(<unicode/uversion.h>)
	{
		U_ICU_VERSION_MAJOR_NUM,
		U_ICU_VERSION_MINOR_NUM,
		U_ICU_VERSION_PATCHLEVEL_NUM,
	},
	U_ICU_VERSION
	#endif
};

decltype(ircd::icu::version_abi)
ircd::icu::version_abi
{
	"icu", info::versions::ABI, 0, {0}, []
	(auto &v, const mutable_buffer &s)
	{
		#if __has_include(<unicode/uversion.h>)
		UVersionInfo info;
		u_getVersion(info);
		u_versionToString(info, data(s));
		#endif
	}
};

decltype(ircd::icu::unicode_version_api)
ircd::icu::unicode_version_api
{
	"unicode", info::versions::API, 0, {0},
	#if __has_include(<unicode/uchar.h>)
	U_UNICODE_VERSION
	#endif
};

decltype(ircd::icu::unicode_version_abi)
ircd::icu::unicode_version_abi
{
	"unicode", info::versions::ABI, 0, {0}, []
	(auto &v, const mutable_buffer &s)
	{
		#if __has_include(<unicode/uchar.h>)
		UVersionInfo info;
		u_getUnicodeVersion(info);
		u_versionToString(info, data(s));
		#endif
	}
};

//
// uchar
//

#if __has_include(<unicode/uchar.h>)

ircd::string_view
ircd::icu::property_acronym(const uint &prop)
{
	return u_getPropertyName(UProperty(prop), U_SHORT_PROPERTY_NAME);
}

ircd::string_view
ircd::icu::property_name(const uint &prop)
{
	return u_getPropertyName(UProperty(prop), U_LONG_PROPERTY_NAME);
}

ircd::string_view
ircd::icu::name(const mutable_buffer &out,
                const char32_t &ch)
{
	UErrorCode err{U_ZERO_ERROR};
	const auto len
	{
		u_charName(ch, U_EXTENDED_CHAR_NAME, data(out), size(out), &err)
	};

	if(unlikely(U_FAILURE(err)))
		throw error
		{
			"%s", u_errorName(err)
		};

	return string_view
	{
		data(out), std::min(size_t(len), size(out))
	};
}

ircd::string_view
ircd::icu::name(const mutable_buffer &out,
                std::nothrow_t,
                const char32_t &ch)
{
	UErrorCode err{U_ZERO_ERROR};
	const auto len
	{
		u_charName(ch, U_EXTENDED_CHAR_NAME, data(out), size(out), &err)
	};

	return string_view
	{
		data(out), U_SUCCESS(err)? std::min(size_t(len), size(out)): 0UL
	};
}

char32_t
ircd::icu::name(const string_view &name)
{
	thread_local char buf[128];
	UErrorCode err{U_ZERO_ERROR};
	const auto ret
	{
		u_charFromName(U_EXTENDED_CHAR_NAME, data(strlcpy(buf, name)), &err)
	};

	if(unlikely(U_FAILURE(err)))
		throw error
		{
			"%s", u_errorName(err)
		};

	return ret;
}

char32_t
ircd::icu::name(std::nothrow_t,
                const string_view &name)
{
	thread_local char buf[128];
	UErrorCode err{U_ZERO_ERROR};
	const auto ret
	{
		u_charFromName(U_EXTENDED_CHAR_NAME, data(strlcpy(buf, name)), &err)
	};

	return U_SUCCESS(err)? ret: char32_t(0xfffd);
}

char32_t
ircd::icu::tolower(const char32_t &ch)
noexcept
{
	return u_tolower(ch);
}

char32_t
ircd::icu::toupper(const char32_t &ch)
noexcept
{
	return u_toupper(ch);
}

ircd::u32x16
ircd::icu::is_char(const c32x16 ch)
noexcept
{
	u32x16 ret{0};
	for(size_t i{0}; i < 16; ++i)
		ret[i] = boolmask<u32>(is_char(ch[i]));

	return ret;
}

ircd::u32x16
ircd::icu::is_nonchar(const c32x16 ch)
noexcept
{
	u32x16 ret{0};
	for(size_t i{0}; i < 16; ++i)
		ret[i] = boolmask<u32>(is_nonchar(ch[i]));

	return ret;
}

ircd::i32x16
ircd::icu::block(const c32x16 ch)
noexcept
{
	i32x16 ret{0};
	for(size_t i{0}; i < 16; ++i)
		ret[i] = ch[i]? (1U << block(ch[i])): 0;

	return ret;
}

ircd::i32x16
ircd::icu::category(const c32x16 ch)
noexcept
{
	i32x16 ret{0};
	for(size_t i{0}; i < 16; ++i)
		ret[i] = ch[i]? (1U << category(ch[i])): 0;

	return ret;
}

bool
ircd::icu::is_char(const char32_t &ch)
noexcept
{
	return U_IS_UNICODE_CHAR(ch);
}

bool
ircd::icu::is_nonchar(const char32_t &ch)
noexcept
{
	return U_IS_UNICODE_NONCHAR(ch);
}

int16_t
ircd::icu::block(const char32_t &ch)
noexcept
{
	return ublock_getCode(ch);
}

int8_t
ircd::icu::category(const char32_t &ch)
noexcept
{
	return u_charType(ch);
}

#endif // __has_include(<unicode/uchar.h>)

//
// utf-16
//

#if __has_include(<unicode/utf16.h>)

char32_t
ircd::icu::utf16::get_unsafe(const string_view &in)
noexcept
{
	UChar32 ret;
	const auto &_in(reinterpret_cast<const uint8_t *>(data(in)));
	U16_GET_UNSAFE(_in, 0, ret);
	return ret;
}

char32_t
ircd::icu::utf16::get_or_fffd(const string_view &in_)
noexcept
{
	UChar32 ret;
	const auto &in(reinterpret_cast<const uint8_t *>(data(in_)));
	const int32_t len(size(in_));
	U16_GET_OR_FFFD(in, 0, 0, len, ret);
	return ret;
}

char32_t
ircd::icu::utf16::get(const string_view &in_)
noexcept
{
	UChar32 ret;
	const auto &in(reinterpret_cast<const uint8_t *>(data(in_)));
	const int32_t len(size(in_));
	U16_GET(in, 0, 0, len, ret);
	return ret;
}

size_t
ircd::icu::utf16::length(const string_view &in)
noexcept
{
	return utf16::length(utf16::get(in));
}

size_t
ircd::icu::utf16::length(const char32_t &ch)
noexcept
{
	return U16_LENGTH(ch);
}

bool
ircd::icu::utf16::single(const char &ch)
noexcept
{
	return U16_IS_SINGLE(ch);
}

bool
ircd::icu::utf16::trail(const char &ch)
noexcept
{
	return U16_IS_TRAIL(ch);
}

bool
ircd::icu::utf16::lead(const char &ch)
noexcept
{
	return U16_IS_LEAD(ch);
}

#endif // __has_include(<unicode/utf16.h>)

//
// utf-8
//

#if __has_include(<unicode/utf8.h>)

ircd::const_buffer
ircd::icu::utf8::encode(const mutable_buffer &out,
                        const vector_view<const char32_t> &in)
{
	const auto &_out
	{
		reinterpret_cast<uint8_t *>(data(out))
	};

	size_t off(0);
	for(size_t i(0); i < in.size() && off + 4 < size(out); ++i)
		U8_APPEND_UNSAFE(_out, off, in[i]);

	assert(off <= size(out));
	return const_buffer
	{
		out, off
	};
}

size_t
ircd::icu::utf8::decode(char32_t *const &out,
                        const size_t &max,
                        const string_view &in)
{
	const auto &_in
	{
		reinterpret_cast<const uint8_t *>(data(in))
	};

	size_t ret(0);
	int32_t off(0);
	const int32_t size_in(size(in));
	for(; ret < max && off < size_in; ++ret)
		U8_NEXT(_in, off, size_in, out[ret]);

	assert(off <= size_in);
	assert(ret <= max);
	return ret;
}

char32_t
ircd::icu::utf8::get_unsafe(const string_view &in)
noexcept
{
	UChar32 ret;
	const auto &_in(reinterpret_cast<const uint8_t *>(data(in)));
	U8_GET_UNSAFE(_in, 0, ret);
	return ret;
}

char32_t
ircd::icu::utf8::get_or_fffd(const string_view &in_)
noexcept
{
	UChar32 ret;
	const auto &in(reinterpret_cast<const uint8_t *>(data(in_)));
	const int32_t len(size(in_));
	U8_GET_OR_FFFD(in, 0, 0, len, ret);
	return ret;
}

char32_t
ircd::icu::utf8::get(const string_view &in_)
noexcept
{
	UChar32 ret;
	const auto &in(reinterpret_cast<const uint8_t *>(data(in_)));
	const int32_t len(size(in_));
	U8_GET(in, 0, 0, len, ret);
	return ret;
}

size_t
ircd::icu::utf8::length(const string_view &in)
noexcept
{
	return utf8::length(utf8::get(in));
}

size_t
ircd::icu::utf8::length(const char32_t &ch)
noexcept
{
	return U8_LENGTH(ch);
}

bool
ircd::icu::utf8::single(const char &ch)
noexcept
{
	return U8_IS_SINGLE(ch);
}

bool
ircd::icu::utf8::trail(const char &ch)
noexcept
{
	return U8_IS_TRAIL(ch);
}

bool
ircd::icu::utf8::lead(const char &ch)
noexcept
{
	return U8_IS_LEAD(ch);
}

#endif // __has_include(<unicode/utf8.h>)
