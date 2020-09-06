// The Construct
//
// Copyright (C) The Construct Developers, Authors & Contributors
// Copyright (C) 2016-2020 Jason Volk <jason@zemos.net>
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

// This prevents spirit grammar rules from generating a very large and/or deep
// call-graph where rules compose together using wrapped indirect calls through
// boost::function -- this is higly inefficient as the grammar's logic ends up
// being a fraction of the generated code and the rest is invocation related
// overhead. By force-flattening here we can allow each entry-point into
// spirit to compose rules at once and eliminate the wrapping complex.
#pragma clang attribute push ([[gnu::always_inline]], apply_to = function)
#pragma clang attribute push ([[gnu::flatten]], apply_to = function)

#include <boost/config.hpp>
#include <boost/function.hpp>

#pragma GCC visibility push (internal)

namespace boost::spirit
{
	using std::function;
}

#include <boost/fusion/sequence.hpp>
#include <boost/fusion/iterator.hpp>
#include <boost/fusion/adapted.hpp>
#include <boost/fusion/functional.hpp>
#include <boost/fusion/algorithm.hpp>
#include <boost/fusion/include/std_pair.hpp>
#include <boost/spirit/include/qi.hpp>
#include <boost/spirit/include/karma.hpp>
#include <boost/spirit/include/phoenix.hpp>
#include <boost/spirit/repository/include/qi_seek.hpp>
#include <boost/spirit/repository/include/qi_subrule.hpp>

#pragma GCC visibility pop
#pragma clang attribute pop
#pragma clang attribute pop

#include <ircd/spirit/spirit.h>
#include <ircd/spirit/expectation_failure.h>
#include <ircd/spirit/parse.h>
#include <ircd/spirit/generate.h>

namespace ircd
{
	using spirit::generate;
	using spirit::parse;
}

#endif HAVE_IRCD_SPIRIT_H
