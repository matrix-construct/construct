// The Construct
//
// Copyright (C) The Construct Developers, Authors & Contributors
// Copyright (C) 2016-2021 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

#include <ircd/matrix.h>
#include "construct.h"
#include "homeserver.h"

construct::homeserver::homeserver(ircd::matrix &matrix,
                                  struct ircd::m::homeserver::opts opts)
try
:matrix
{
	matrix
}
,opts
{
	std::move(opts)
}
,hs
{
	matrix.init(&this->opts),
	[&matrix](ircd::m::homeserver *const hs)
	{
		matrix.fini(hs);
	}
}
{
}
catch(const std::exception &e)
{
	// Rethrow as ircd::error because we're about to unload the module
	// and all m:: type exceptions won't exist anymore...
	throw ircd::error
	{
		"%s", e.what()
	};
}

construct::homeserver::~homeserver()
noexcept
{
}
