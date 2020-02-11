// Matrix Construct
//
// Copyright (C) Matrix Construct Developers, Authors & Contributors
// Copyright (C) 2016-2020 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

#include <pbc.h>

namespace ircd::pbc
{
	static void init() __attribute__((constructor));

	extern info::versions version_api, version_abi;
}

decltype(ircd::pbc::version_api)
ircd::pbc::version_api
{
	"pbc", info::versions::API, 0
};

decltype(ircd::pbc::version_abi)
ircd::pbc::version_abi
{
	"pbc", info::versions::ABI, 0
};

void
ircd::pbc::init()
{

}
