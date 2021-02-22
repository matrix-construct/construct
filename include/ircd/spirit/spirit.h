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
#define HAVE_IRCD_SPIRIT_SPIRIT_H

namespace ircd {
namespace spirit
__attribute__((visibility("default")))
{
	IRCD_EXCEPTION(ircd::error, error);
}}

namespace ircd {
namespace spirit
__attribute__((visibility("internal")))
{
	namespace phx = boost::phoenix;
	namespace fusion = boost::fusion;
	namespace spirit = boost::spirit;
	namespace ascii = spirit::ascii;
	namespace karma = spirit::karma;
	namespace qi = spirit::qi;

	using _val_type = phx::actor<spirit::attribute<0>>;
	using _r0_type = phx::actor<spirit::attribute<0>>;
	using _r1_type = phx::actor<spirit::attribute<1>>;
	using _r2_type = phx::actor<spirit::attribute<2>>;
	using _r3_type = phx::actor<spirit::attribute<3>>;

	using spirit::unused_type;
	using spirit::auto_;
	using spirit::_pass;
	using spirit::_val;
	using spirit::inf;

	using qi::locals;
	using qi::_a;
	using qi::_a_type;
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
	using karma::skip;

	template<size_t idx,
	         class semantic_context>
	auto &
	attr_at(semantic_context&&);

	template<size_t idx,
	         class semantic_context>
	auto &
	local_at(semantic_context&&);
}}

namespace ircd::spirit::local
{
	using qi::_0;
	using qi::_1;
	using qi::_2;
	using qi::_3;
}

template<size_t idx,
         class semantic_context>
inline auto &
ircd::spirit::local_at(semantic_context&& c)
{
	return boost::fusion::at_c<idx>(c.locals);
}

template<size_t idx,
         class semantic_context>
inline auto &
ircd::spirit::attr_at(semantic_context&& c)
{
	return boost::fusion::at_c<idx>(c.attributes);
}
