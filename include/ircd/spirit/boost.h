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
#pragma clang system_header
#define HAVE_IRCD_SPIRIT_BOOST_H

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
#include "function.h"

#pragma GCC visibility push (internal)
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
#include "char.h"
#pragma GCC visibility pop

#pragma clang attribute pop
#pragma clang attribute pop
