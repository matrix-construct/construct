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

namespace fs = ircd::fs;
using ircd::string_view;

decltype(construct::homeserver::primary)
construct::homeserver::primary;

construct::homeserver::homeserver(struct ircd::m::homeserver::opts opts)
try
:opts
{
	std::move(opts)
}
,module_path
{
	fs::path_string(fs::path_views{fs::base::lib, "libircd_matrix"}),
}
,module
{
	string_view{module_path[0]},
}
,init
{
	module[0], "ircd::m::homeserver::init",
}
,fini
{
	module[0], "ircd::m::homeserver::fini",
}
,hs
{
	this->init(&this->opts), [this](ircd::m::homeserver *const hs)
	{
		this->fini(hs);
	}
}
{
	assert(!primary);
	primary = this;
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
	assert(primary);
	primary = nullptr;
}
