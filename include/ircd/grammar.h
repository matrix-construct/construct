// Matrix Construct
//
// Copyright (C) Matrix Construct Developers, Authors & Contributors
// Copyright (C) 2016-2019 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

#pragma once
#define HAVE_IRCD_GRAMMAR_H

// This header forward declares certain boost::spirit symbols in the ircd.h
// standard include group because we do not include boost headers. This allows
// our headers to make extern references between compilation units without
// separate/internal headers. For example, one can create different grammars
// in separate compilation units and share individual rules between them,
// avoiding any DRY.

namespace boost::spirit
{
	struct unused_type;
}

namespace boost::spirit::qi
{
	template<class it,
	         class T1,
	         class T2,
	         class T3,
	         class T4>
	struct rule;
}
