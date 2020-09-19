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
#include <ircd/spirit/boost.h>
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
