// The Construct
//
// Copyright (C) The Construct Developers, Authors & Contributors
// Copyright (C) 2016-2021 Jason Volk <jason@zemos.net>
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice is present in all copies. The
// full license for this software is available in the LICENSE file.

struct construct::homeserver
{
	ircd::matrix &matrix;
	struct ircd::m::homeserver::opts opts;
	ircd::custom_ptr<ircd::m::homeserver> hs;

  public:
	homeserver(ircd::matrix &, struct ircd::m::homeserver::opts);
	~homeserver() noexcept;
};
