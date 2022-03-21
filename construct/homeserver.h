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
	using init_proto = ircd::m::homeserver *(const struct ircd::m::homeserver::opts *);
	using fini_proto = void (ircd::m::homeserver *);

	struct ircd::m::homeserver::opts opts;
	std::string module_path[1];
	ircd::module module[1];
	ircd::mods::import<init_proto> init;
	ircd::mods::import<fini_proto> fini;
	ircd::custom_ptr<ircd::m::homeserver> hs;

  public:
	homeserver(struct ircd::m::homeserver::opts);
	~homeserver() noexcept;
};
