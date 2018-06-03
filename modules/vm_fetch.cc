// Matrix Construct
//
// Copyright (C) Matrix Construct Developers, Authors & Contributors
// Copyright (C) 2016-2018 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

using namespace ircd;

m::vm::phase
fetch_phase
{
	"fetch"
};

void init()
void fini();

mapi::header
IRCD_MODULE
{
	"Matrix Virtual Machine: fetch unit",
	init, fini
};

//
// init
//

void
init()
{
}

void
fini()
{
}
