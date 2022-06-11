// The Construct
//
// Copyright (C) The Construct Developers, Authors & Contributors
// Copyright (C) 2016-2022 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

#pragma once
#define HAVE_IRCD_SPIRIT_QI_CHAR_H

#if defined(__clang__)
template<class Derived>
struct [[clang::internal_linkage, gnu::visibility("internal")]]
boost::spirit::qi::char_parser
<
	Derived,
	typename boost::spirit::char_encoding::standard::char_type,
	typename boost::mpl::if_c<true, boost::spirit::unused_type, typename boost::spirit::char_encoding::standard::char_type>::type
>
:primitive_parser
<
	literal_char<char_encoding::standard, true, false>
>
{
	using CharEncoding = boost::spirit::char_encoding::standard;
	using char_type = typename CharEncoding::char_type;
	struct char_parser_id;

	template<class Context,
	         class Iterator>
	struct attribute
	{
		using type = char_type;
	};

	template<class Iterator,
	         class Context,
	         class Skipper,
	         class Attribute>
	[[gnu::hot]]
	bool parse(Iterator &__restrict__ start, const Iterator &__restrict__ stop, Context &c, const Skipper &s, Attribute &a) const
	{
		qi::skip_over(start, stop, s);
		if(likely(start != stop))
		{
			if(this->derived().test(*start, c))
			{
				spirit::traits::assign_to(*start, a);
				++start;
				return true;
			}
			else return false;
		}
		else return false;
	}
};
#endif

#if defined(__clang__)
template<>
struct [[clang::internal_linkage, gnu::visibility("internal")]]
boost::spirit::qi::literal_char<boost::spirit::char_encoding::standard, true, false>
:char_parser
<
	literal_char<char_encoding::standard, true, false>,
    typename char_encoding::standard::char_type,
	typename mpl::if_c<true, unused_type, typename char_encoding::standard::char_type>::type
>
{
	using CharEncoding = boost::spirit::char_encoding::standard;
	using char_type = typename CharEncoding::char_type;
	using char_encoding = CharEncoding;

	template<class Context,
	         class Iterator>
	struct attribute
	{
		using type = typename mpl::if_c<true, unused_type, char_type>::type;
	};

  private:
	const char_type ch alignas(16);

  public:
	template<class CharParam,
	         class Context>
	[[gnu::hot]]
	bool test(const CharParam ch, Context &) const
	{
		static_assert(std::is_same<CharParam, char_type>::value);

		#ifndef NDEBUG
		ircd::always_assert(traits::ischar<CharParam, char_encoding>::call(ch));
		#endif

		return this->ch == ch;
	}

	template<class Context>
	info what(Context &) const
	{
		return info
		{
			"literal-char", char_encoding::toucs4(ch)
		};
	}

	constexpr
	literal_char(const char ch)
	:ch(ch)
	{}
};
#endif

#if defined(__clang__)
template<>
struct [[clang::internal_linkage, gnu::visibility("internal")]]
boost::spirit::qi::char_range<boost::spirit::char_encoding::standard, false>
:char_parser
<
	char_range<char_encoding::standard, false>,
    typename char_encoding::standard::char_type
>
{
	using CharEncoding = boost::spirit::char_encoding::standard;
	using char_type = typename CharEncoding::char_type;
	using char_encoding = CharEncoding;

  private:
	const char_type from alignas(16);
	const char_type to alignas(16);

  public:
	template<class CharParam,
	         class Context>
	[[gnu::hot]]
	bool test(const CharParam ch_, Context &) const
	{
		using ischar = typename traits::ischar<CharParam, char_encoding>;
		using ircd::boolmask;

		#ifndef NDEBUG
		ircd::always_assert(ischar::call(ch_));
		#endif

		const char_type ch(ch_);
		const char_type res[2]
		{
			boolmask<char_type>(ch >= from),
			boolmask<char_type>(ch <= to),
		};

		return res[0] & res[1];
	}

	template<class Context>
	info what(Context &) const
	{
		return info {"char-range"};
	}

	constexpr
	char_range(const char_type from,
	           const char_type to)
	:from{from}
	,to{to}
	{
		assert(from <= to);
	}
};
#endif
