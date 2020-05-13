// Matrix Construct
//
// Copyright (C) Matrix Construct Developers, Authors & Contributors
// Copyright (C) 2016-2018 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

#ifndef HAVE_IRCD_SPIRIT_H
#define HAVE_IRCD_SPIRIT_H

/// This file is not part of the IRCd standard include list (stdinc.h) because
/// it involves extremely expensive boost headers for creating formal spirit
/// grammars. Include this in a definition file which defines such grammars.
///
/// Note that directly sharing elements of a grammar between two compilation
/// units can be achieved with forward declarations in `ircd/grammar.h`.

// ircd.h is included here so that it can be compiled into this header. Then
// this becomes the single leading precompiled header.
#include <ircd/ircd.h>

// Disables asserts in spirit headers even when we're NDEBUG due to
// some false asserts around boolean character tests in spirit.
#define BOOST_DISABLE_ASSERTS

#pragma GCC visibility push(default)
#include <boost/config.hpp>
#include <boost/function.hpp>
#pragma GCC visibility pop

#pragma GCC visibility push(hidden)
#include <boost/fusion/include/at.hpp>
#include <boost/spirit/include/qi.hpp>
#include <boost/spirit/include/karma.hpp>
#include <boost/spirit/include/phoenix.hpp>
#include <boost/spirit/repository/include/qi_seek.hpp>
#include <boost/spirit/repository/include/qi_subrule.hpp>
#include <boost/fusion/include/std_pair.hpp>
#include <boost/fusion/adapted/std_pair.hpp>
#include <boost/fusion/adapted/std_tuple.hpp>
#pragma GCC visibility pop

namespace ircd {
namespace spirit
__attribute__((visibility("default")))
{
	template<class parent_error> struct expectation_failure;

	extern thread_local char rulebuf[64]; // parse.cc
}}

namespace ircd
__attribute__((visibility("default")))
{
	template<class parent_error,
	         class it = const char *,
	         class... args>
	bool parse(args&&...);
}

namespace ircd {
namespace spirit
__attribute__((visibility("hidden")))
{
	namespace spirit = boost::spirit;
	namespace ascii = spirit::ascii;
	namespace karma = spirit::karma;
	namespace qi = spirit::qi;
	namespace phx = boost::phoenix;

	using _val_type = phx::actor<spirit::attribute<0>>;
	using _r0_type = phx::actor<spirit::attribute<0>>;
	using _r1_type = phx::actor<spirit::attribute<1>>;
	using _r2_type = phx::actor<spirit::attribute<2>>;
	using _r3_type = phx::actor<spirit::attribute<3>>;

	using spirit::unused_type;
	using spirit::_pass;

	using qi::locals;
	using qi::_a;
	using qi::_r1_type;
	using qi::raw;
	using qi::omit;
	using qi::matches;
	using qi::hold;
	using qi::eoi;
	using qi::eps;
	using qi::expect;
	using qi::attr;
	using qi::attr_cast;
	using qi::repeat;
	using qi::lit;
	using qi::char_;
	using qi::byte_;
	using qi::string;
	using qi::short_;
	using qi::ushort_;
	using qi::word;
	using qi::big_word;
	using qi::little_word;
	using qi::int_;
	using qi::uint_;
	using qi::dword;
	using qi::big_dword;
	using qi::little_dword;
	using qi::long_;
	using qi::ulong_;
	using qi::qword;
	using qi::big_qword;
	using qi::little_qword;
	using qi::float_;
	using qi::bin_float;
	using qi::big_bin_float;
	using qi::little_bin_float;
	using qi::double_;
	using qi::bin_double;
	using qi::big_bin_double;
	using qi::little_bin_double;

	using spirit::repository::qi::seek;

	using karma::lit;
	using karma::char_;
	using karma::long_;
	using karma::double_;
	using karma::bool_;
	using karma::eps;
	using karma::attr_cast;
	using karma::maxwidth;
	using karma::buffer;
}}

namespace ircd::spirit::local
{
	using qi::_1;
	using qi::_2;
	using qi::_3;
}

namespace ircd {
namespace spirit
__attribute__((visibility("default")))
{
}}

template<class parent_error>
struct __attribute__((visibility("default")))
ircd::spirit::expectation_failure
:parent_error
{
	template<class it = const char *>
	expectation_failure(const qi::expectation_failure<it> &e,
	                    const ssize_t &show_max = 64);

	template<class it = const char *>
	expectation_failure(const qi::expectation_failure<it> &e,
	                    const it &start,
	                    const ssize_t &show_max = 64);
};

template<class parent>
template<class it>
ircd::spirit::expectation_failure<parent>::expectation_failure(const qi::expectation_failure<it> &e,
		                                                       const ssize_t &show_max)
:parent
{
	"Expected %s. You input %zd invalid characters :%s",
	ircd::string(rulebuf, e.what_),
	std::distance(e.first, e.last),
	string_view{e.first, e.first + std::min(std::distance(e.first, e.last), show_max)}
}
{}

template<class parent>
template<class it>
ircd::spirit::expectation_failure<parent>::expectation_failure(const qi::expectation_failure<it> &e,
                                                               const it &start,
		                                                       const ssize_t &show_max)
:parent
{
	"Expected %s. You input %zd invalid characters somewhere between position %zd and %zd: %s",
	ircd::string(rulebuf, e.what_),
	std::distance(e.first, e.last),
	std::distance(start, e.first),
	std::distance(start, e.last),
	string_view{e.first, e.first + std::min(std::distance(e.first, e.last), show_max)}
}
{}

template<class parent_error,
         class it,
         class... args>
inline bool
ircd::parse(args&&... a)
try
{
	return spirit::qi::parse(std::forward<args>(a)...);
}
catch(const spirit::qi::expectation_failure<it> &e)
{
	throw spirit::expectation_failure<parent_error>(e);
}

#endif // HAVE_IRCD_SPIRIT_H
