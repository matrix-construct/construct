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
#define HAVE_IRCD_SPIRIT_CUSTOM_PARSER_H

// This file is not part of the IRCd standard include list (stdinc.h) because
// it involves extremely expensive boost headers for creating formal spirit
// grammars. This file is automatically included in the spirit.h group.

namespace ircd {
namespace spirit
__attribute__((visibility("internal")))
{
	template<size_t id> struct custom_parser;
	BOOST_SPIRIT_TERMINAL(custom0);
	BOOST_SPIRIT_TERMINAL(custom1);
	BOOST_SPIRIT_TERMINAL(custom2);
}}

namespace boost {
namespace spirit
__attribute__((visibility("internal")))
{
	namespace qi
	{
		template<class modifiers>
		struct make_primitive<ircd::spirit::tag::custom0, modifiers>;

		template<class modifiers>
		struct make_primitive<ircd::spirit::tag::custom1, modifiers>;

		template<class modifiers>
		struct make_primitive<ircd::spirit::tag::custom2, modifiers>;
	}

	template<>
	struct [[clang::internal_linkage]]
	use_terminal<qi::domain, ircd::spirit::tag::custom0>
	:mpl::true_
	{};

	template<>
	struct [[clang::internal_linkage]]
	use_terminal<qi::domain, ircd::spirit::tag::custom1>
	:mpl::true_
	{};

	template<>
	struct [[clang::internal_linkage]]
	use_terminal<qi::domain, ircd::spirit::tag::custom2>
	:mpl::true_
	{};
}}

template<size_t id>
struct [[gnu::visibility("internal"), clang::internal_linkage]]
ircd::spirit::custom_parser
:qi::primitive_parser<custom_parser<id>>
{
	template<class context,
	         class iterator>
	struct attribute
	{
		using type = iterator;
	};

	template<class context>
	boost::spirit::info what(context &) const
	{
		return boost::spirit::info("custom");
	}

	template<class iterator,
	         class context,
	         class skipper,
	         class attr>
	[[clang::internal_linkage]]
	bool parse(iterator &, const iterator &, context &, const skipper &, attr &) const;
};

template<class modifiers>
struct [[gnu::visibility("internal"), clang::internal_linkage]]
boost::spirit::qi::make_primitive<ircd::spirit::tag::custom0, modifiers>
{
	using result_type = ircd::spirit::custom_parser<0>;

	result_type operator()(unused_type, unused_type) const
	{
		return result_type{};
	}
};

template<class modifiers>
struct [[gnu::visibility("internal"), clang::internal_linkage]]
boost::spirit::qi::make_primitive<ircd::spirit::tag::custom1, modifiers>
{
	using result_type = ircd::spirit::custom_parser<1>;

	result_type operator()(unused_type, unused_type) const
	{
		return result_type{};
	}
};

template<class modifiers>
struct [[gnu::visibility("internal"), clang::internal_linkage]]
boost::spirit::qi::make_primitive<ircd::spirit::tag::custom2, modifiers>
{
	using result_type = ircd::spirit::custom_parser<2>;

	result_type operator()(unused_type, unused_type) const
	{
		return result_type{};
	}
};
