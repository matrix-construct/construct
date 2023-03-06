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

// These trip include guards on some headers which pollute the unit such as
// those which include <iostream>, etc. These headers contain static objects
// and initializations which prevent optimization of our static grammars.
#if defined(__clang__)
#define BOOST_PROTO_DEBUG_HPP_EAN_12_31_2006
#define BOOST_SPIRIT_DEBUG_HANDLER_DECEMBER_05_2008_0734PM
#define BOOST_SPIRIT_SIMPLE_TRACE_DECEMBER_06_2008_1102AM
#define BOOST_SPIRIT_KARMA_DEBUG_HANDLER_APR_21_2010_0148PM
#define BOOST_SPIRIT_KARMA_SIMPLE_TRACE_APR_21_2010_0155PM
#define BOOST_PHOENIX_DEBUG_HPP
#endif

// Multiple definitions from phoenix tuple impl with gcc+ld.gold; doesn't seem
// to be a necessary header for our uses, so we guard it out.
#if !defined(__clang__)
#define BOOST_PHOENIX_STL_TUPLE_H_
#endif

// These must be included prior to the internal visibility/linkage sections.
#include <boost/config.hpp>
#include <boost/type_index.hpp>
#include <boost/function/function_fwd.hpp>

// Upstream includes for spirit; these would otherwise generate layers of
// template noise in the symbol table but they can be strongly hidden.
#pragma GCC visibility push(internal)
#pragma clang attribute push(__attribute__((internal_linkage)), apply_to=any(function,record,variable))
#include <boost/none.hpp>
#include "function.h"
#include <boost/fusion/sequence.hpp>
#include <boost/fusion/iterator.hpp>
#include <boost/fusion/adapted.hpp>
#include <boost/fusion/functional.hpp>
#include <boost/fusion/algorithm.hpp>
#include <boost/fusion/include/std_pair.hpp>
#include <boost/proto/traits.hpp>
#include <boost/proto/extends.hpp>
#include <boost/proto/operators.hpp>
#include <boost/proto/tags.hpp>
#include <boost/proto/proto_fwd.hpp>
#include <boost/proto/make_expr.hpp>
#include <boost/proto/transform.hpp>
#include <boost/proto/deep_copy.hpp>
#include <boost/phoenix.hpp>
#include "expr.h"
#pragma clang attribute pop
#pragma GCC visibility pop

// Spirit includes
#pragma GCC visibility push(protected)
#include <boost/spirit/include/support.hpp>
#include <boost/spirit/include/qi.hpp>
#include "qi_rule.h"
#include "qi_char.h"
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdangling-pointer"
#include <boost/spirit/include/karma.hpp>
#pragma GCC diagnostic pop
#include "karma_rule.h"
#include <boost/spirit/repository/include/qi_seek.hpp>
#include <boost/spirit/repository/include/qi_subrule.hpp>
#pragma GCC visibility pop
