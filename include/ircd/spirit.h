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
#define HAVE_IRCD_SPIRIT_H

/// This file is not part of the IRCd standard include list (stdinc.h) because
/// it involves extremely expensive boost headers for creating formal spirit
/// grammars. Include this in a definition file which defines such grammars.

#include <boost/spirit/include/qi.hpp>
#include <boost/spirit/include/karma.hpp>
#include <boost/spirit/include/phoenix.hpp>
#include <boost/fusion/include/std_pair.hpp>
#include <boost/fusion/adapted/std_pair.hpp>
#include <boost/fusion/adapted/std_tuple.hpp>

namespace ircd::spirit
{
	namespace spirit = boost::spirit;
	namespace ascii = spirit::ascii;
	namespace karma = spirit::karma;
	namespace qi = spirit::qi;

	using spirit::unused_type;

	using qi::lit;
	using qi::char_;
	using qi::string;
	using qi::short_;
	using qi::ushort_;
	using qi::int_;
	using qi::uint_;
	using qi::long_;
	using qi::ulong_;
	using qi::double_;
	using qi::raw;
	using qi::omit;
	using qi::matches;
	using qi::hold;
	using qi::eoi;
	using qi::eps;
	using qi::attr;
	using qi::attr_cast;
	using qi::repeat;
	using qi::_r1_type;

	using karma::lit;
	using karma::char_;
	using karma::long_;
	using karma::double_;
	using karma::bool_;
	using karma::eps;
	using karma::attr_cast;
	using karma::maxwidth;
	using karma::buffer;
}
