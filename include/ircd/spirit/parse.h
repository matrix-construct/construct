// The Construct
//
// Copyright (C) The Construct Developers, Authors & Contributors
// Copyright (C) 2016-2020 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

#pragma once
#define HAVE_IRCD_SPIRIT_PARSE_H

namespace ircd {
namespace spirit
__attribute__((visibility("internal")))
{
	struct substring_view;

	template<size_t id> struct custom_parser;
	BOOST_SPIRIT_TERMINAL(custom0);
	BOOST_SPIRIT_TERMINAL(custom1);
	BOOST_SPIRIT_TERMINAL(custom2);

	template<class gen,
	         class... attr>
	bool parse(const char *&start, const char *const &stop, gen&&, attr&&...);

	template<class parent_error,
	         size_t error_show_max  = 128,
	         class gen,
	         class... attr>
	bool parse(const char *&start, const char *const &stop, gen&&, attr&&...);
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
	struct use_terminal<qi::domain, ircd::spirit::tag::custom0>
	:mpl::true_
	{};

	template<>
	struct use_terminal<qi::domain, ircd::spirit::tag::custom1>
	:mpl::true_
	{};

	template<>
	struct use_terminal<qi::domain, ircd::spirit::tag::custom2>
	:mpl::true_
	{};
}}

template<size_t id>
struct [[gnu::visibility("internal")]]
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
	bool parse(iterator &, const iterator &, context &, const skipper &, attr &) const;
};

template<class modifiers>
struct [[gnu::visibility("internal")]]
boost::spirit::qi::make_primitive<ircd::spirit::tag::custom0, modifiers>
{
	using result_type = ircd::spirit::custom_parser<0>;

	result_type operator()(unused_type, unused_type) const
	{
		return result_type{};
	}
};

template<class modifiers>
struct [[gnu::visibility("internal")]]
boost::spirit::qi::make_primitive<ircd::spirit::tag::custom1, modifiers>
{
	using result_type = ircd::spirit::custom_parser<1>;

	result_type operator()(unused_type, unused_type) const
	{
		return result_type{};
	}
};

template<class modifiers>
struct [[gnu::visibility("internal")]]
boost::spirit::qi::make_primitive<ircd::spirit::tag::custom2, modifiers>
{
	using result_type = ircd::spirit::custom_parser<2>;

	result_type operator()(unused_type, unused_type) const
	{
		return result_type{};
	}
};

struct ircd::spirit::substring_view
:ircd::string_view
{
	using _iterator = boost::spirit::karma::detail::indirect_iterator<const char *>;
	using _iterator_range = boost::iterator_range<_iterator>;

	using ircd::string_view::string_view;
	explicit substring_view(const _iterator_range &range)
	:ircd::string_view
	{
		std::addressof(*range.begin()), std::addressof(*range.end())
	}
	{}
};

template<class parent_error,
         size_t error_show_max,
         class gen,
         class... attr>
[[using gnu: always_inline, gnu_inline, artificial]]
extern inline bool
ircd::spirit::parse(const char *&start,
                    const char *const &stop,
                    gen&& g,
                    attr&&... a)
try
{
	return qi::parse(start, stop, std::forward<gen>(g), std::forward<attr>(a)...);
}
catch(const qi::expectation_failure<const char *> &e)
{
	throw expectation_failure<parent_error>
	{
		e, error_show_max
	};
}

template<class gen,
         class... attr>
[[using gnu: always_inline, gnu_inline, artificial]]
extern inline bool
ircd::spirit::parse(const char *&start,
                    const char *const &stop,
                    gen&& g,
                    attr&&... a)
{
	return qi::parse(start, stop, std::forward<gen>(g), std::forward<attr>(a)...);
}
